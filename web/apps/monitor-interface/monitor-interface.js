/*
 * An application for monitoring interfaces on a JUNOS device.
 *
 * Gathers interface information and statistics from a JUNOS device. Provides
 * interface utilization graph and statistics like current, peak and average
 * rates of traffic for a selected physical or logical interface. Also shows
 * error stats on physical interfaces.
 */

/* Dynamically load our CSS dependencies. This is temporary and will go away
 * once css files are added to the fold of dynamically picked files.
 */
var files = [
    "/external/selectric/selectric.css",
    "/apps/monitor-interface/css/monitor-interface.css"
]
files.forEach (function (file) {
    var o = document.createElement("link");
    o.setAttribute("rel", "stylesheet");
    o.setAttribute("href", file);
    document.getElementsByTagName("head")[0].appendChild(o);
});


$(function($){
    $.clira.commandFile({
        name : "monitor-interface",
        templatesFile : "/apps/monitor-interface/monitor-interface.hbs",
        prereqs : [
            "/external/smoothie/smoothie.js",
            "/external/selectric/selectric.js"
        ],
        commands : [
            {
                command : "monitor interface",
                templateName : "monitor-interface",
                arguments : [
                    { 
                        name : "interfaceName",
                        help : "Interface name",
                        type : "string",
                        nokeyword : true,
                        mandatory : true
                    } ,
                    {
                        name : "target",
                        help : "Target device",
                        type : "string",
                        nokeyword : true,
                        mandatory : true
                    }
                ],
                execute: function (view, cmd, parse, poss) {
                    target = poss.data.target;
                    interfaceName = poss.data.interfaceName;
                    /*
                     * Create an instance and kickstart the monitoring
                     */
                    var monitorInterface = createAppInstance();
                    monitorInterface.init(view, target, interfaceName);
                }
            } 
        ] 
    });
    /*
     * Creates app instances.
     * To avoid confusion: 
     * IFD (Interface device) -> Physical interface (fxp0, ge-0/0/0, etc.)
     * IFL (Interface logical)-> Logical interface (fxp0.0, ge-0/0/0.0, etc.)
     * http://www.juniper.net/documentation/en_US/junos14.2/topics/concept/interfaces-interface-naming-overview.html
     */
    function createAppInstance () {
        var monitorInterfaceApp = {
            currInterface : {
                ifd : null, 
                ifl : null,
                name : null, 
                type : null 
            },
            fetchCount : 0,
            interfaceHasChanged : false,
            ifdList : [],
            view : null,
            device : null,
            timeoutId : null,
            rateUnit : 'Auto',
            appError : null,
            chart : new RealTimeChart(null),
            settings : null,    /* Settings in effect */
            defaultSettings : {
                rateUnit : { 
                    type : 'radio', 
                    val :'Auto' 
                },
                autoScale : { 
                    type : 'radio', 
                    val : 'yes' 
                },
                fetchInterval : {
                    type : 'text', 
                    val : 3, 
                    min : 1, 
                    max : 900,
                    unit : 'secs'
                },
                chartPeriod : { 
                    type : 'text', 
                    val : 600, 
                    min : 10, 
                    max : 3600,
                    unit : 'secs'
                },
                chartWidth : { 
                    type : 'text', 
                    val : 800, 
                    min : 100,
                    max : 1024,
                    unit : 'px'
                },
                chartHeight : {
                    type : 'text', 
                    val : 100,
                    min : 50,
                    max : 200,
                    unit : 'px'
                },
                chartNumLines : { 
                    type : null, 
                    val : 10 
                }
            },
            /*
             * Starts the monitoring process. Initializes app data and calls
             * the fetch routines depending on the input.
             * all -> we fetch an interface list and then fetch stats for the 
             *        first interface on that list
             * ifd -> directly fetch that IFD and its children IFLs
             * ifl -> fetch ifd containing this ifl and then set selected 
             *        interface to this ifl.
             */
            init : function (view, target, interfaceName) {
                this.view = view;
                this.device = target;
                this.currInterface.name = interfaceName;
                this.initSettings();
                initAppViews(this);
                this.updateView({appLoading : true});
                if (interfaceName == "all") {
                    this.fetchInterfaceList();
                } else if (isIfl(interfaceName)) {
                    this.fetchInterfaceList(getIfdFromIfl(interfaceName));
                } else {
                    this.fetchInterfaceStats(interfaceName);
                }
                
            },
            /*
             * Send the JUNOS command to the device and set the callback to 
             * handle the response. We also shove in the app context into 
             * the view object which will be returned in the callback and can
             * be used to call the app's methods. Callbacks are executed in 
             * global scope and lose the context. We restore the context using
             * the view object since its unique to each app instance. 
             */
            doRPC : function (command, callback) {
                this.view['appCtx'] = this;
                $.clira.runCommand(this.view, this.device, command, "json", 
                    callback);      
            },
            /*
             * We run 'show interfaces <interface-name>' command to fetch 
             * all the interfaces and create an IFD list. Each IFD will contain
             * the child IFLs under them. This list is used to render the 
             * select interface select box. Also triggers the 
             * fetchInterfaceStats routine to get the selected interface's 
             * stats.
             */
            fetchInterfaceList : function  (ifd) {
                var command = "show interfaces";
                if (ifd) {
                    command += " " + ifd;
                }
                this.doRPC(command, function (view, status, res) {
                    var result = parseRPC(res);
                    var ctx = view.appCtx, ifdSpeed = 0;
                    if (!isError(status, result, ctx)) {
                        var ifds = result.get('physical-interface', false);
                        ifds.each(function (ifd, index) {
                            ctx.ifdList.push({
                                name : ifd.get('name'),
                                iflList : getIflList(ifd),
                                speed : strToSpeed(ifd.get('speed'))
                            });
                        });
                        if (ctx.currInterface.name == "all")
                            ctx.fetchInterfaceStats(ctx.ifdList[0].name);
                        else 
                            ctx.fetchInterfaceStats(ctx.currInterface.name);
                    } else {
                        ctx.updateView({appError : { error : ctx.appError }});
                        ctx.resetAppTimer();
                    }
                });
            },
            /*
             * Uses the 'show interface statistics detail' command for a 
             * given interface to fetch its stats. Depending on the interface
             * type, will populate the ifl or ifd structure. Will execute in a 
             * periodic fashion until the timer has been reset.
             */
            fetchInterfaceStats : function (interfaceName) {
                /*
                 * reset any existing routine
                 */
                this.resetAppTimer(); 
                var command = "show interfaces statistics " + interfaceName + 
                    " detail";
                if (isIfl(interfaceName)) {
                    this.currInterface.type = 'ifl';
                } else {
                    this.currInterface.type = 'ifd';
                }
                this.currInterface.name = interfaceName;
                this.doRPC(command, function (view, status, response) {
                    var ctx = view.appCtx;
                    if (ctx.hasInterfaceChanged) return;
                    var res = parseRPC(response), 
                    ifd = null, 
                    ifl = null;
                    if (!isError(status, res, ctx)) { 
                        if (ctx.currInterface.type == 'ifd' && 
                                (ifd = ctx.getIfd(res))) {
                            ctx.currInterface.ifd = ifd;
                            if (!ctx.ifdList.length)
                                ctx.ifdList = [{
                                    name : ifd.name,
                                    iflList : ifd.iflList,
                                    speed : ifd.speed
                                }];
                            ctx.chart.plotUsage(ifd);
                            ctx.updateView({
                                appContent : {
                                    ifdList : ctx.ifdList, 
                                    interface : ifd, 
                                    stats : getStats(ifd)
                                }
                            }); 
                        }
                        if (ctx.currInterface.type == 'ifl' &&
                                (ifl = ctx.getIfl(res))) {
                            ctx.currInterface.ifl = ifl;
                            ctx.chart.plotUsage(ifl);
                            ctx.updateView({
                                appContent : {
                                    ifdList : ctx.ifdList, 
                                    interface : ifl, 
                                    stats : getStats(ifl)
                                }
                            });
                        }    
                    } else {
                        ctx.updateView({appError : { error : ctx.appError }});
                        ctx.resetAppTimer();
                    }
                });
                this.timeoutId = setTimeout(this.fetchInterfaceStats.bind(
                    this),(this.settings.fetchInterval.val * 1000), 
                        this.currInterface.name);
            },
            /*
             * Return an ifd given a parsed RPC response
             */
            getIfd : function (res) {
                var ifd = res.get('physical-interface');
                if (!ifd) {
                    return null;
                }
                var errors = {
                    ingress : {},
                    egress : {}
                };
                var ingress = ifd.get('input-error-list');
                var egress = ifd.get('output-error-list');
                if (ingress) {
                    ingress.each(function (e, prop) {
                        errors.ingress[prop] = ingress.get(prop);
                    });
                }
                if (egress) {
                    egress.each(function (e, prop) {
                        errors.egress[prop] = egress.get(prop);
                    });
                }
                return {
                    name : ifd.get('name'),
                    speed : strToSpeed(ifd.get('speed')),
                    mtu : ifd.get('mtu'),
                    ifType : ifd.get('if-type'),
                    linkType : ifd.get('link-type'),
                    flapInfo : ifd.get('interface-flapped'),
                    macAddr : ifd.get('hardware-physical-address'),
                    traffic : this.getTrafficInfo(ifd.get('traffic-statistics')),
                    iflList : getIflList(ifd),
                    errors : errors
                };
            }, 
            /*
             * Return an ifl given a parsed RPC response
             */
            getIfl : function (res) {
                var addressFamily = [],
                ifl = res.get('logical-interface'),
                families;
                if (!ifl) {
                    return null;
                }
                if (families = ifl.get('address-family', false)) {
                    families.each(function(family) {
                        addressFamily.push({
                            name : family.get('address-family-name'),
                            mtu : family.get('mtu'),
                            addresses: getIflAddresses(family)
                        });
                    });
                }
                return {
                    name : ifl.get('name'),
                    encapsulation : ifl.get('encapsulation'),
                    addressFamily : addressFamily,
                    traffic : this.getTrafficInfo(ifl.get('traffic-statistics')),
                    ifdSpeed : getIfdSpeed(this.ifdList, ifl.get('name'))
                };  
            },
            resetAppTimer : function () {
                if (this.timeoutId) {
                    window.clearTimeout(this.timeoutId);
                }             
            },
            /*
             * Extract the traffic statistics of an ifl or ifd structure and
             * compute the peak, current and average stats. Returns an object
             * containing the stats. for each queue input/output
             */
            getTrafficInfo : function (traffic) {
                if (traffic) {
                    inBytes = traffic.get('input-bytes'),
                    outBytes = traffic.get('output-bytes'),
                    inRate = {current : 0, average : 0, peak : 0},
                    outRate = {current : 0, average : 0, peak : 0};
                    if (this.currInterface.type == 'ifd')
                        var currIf = this.currInterface.ifd;
                    else
                        var currIf = this.currInterface.ifl;
                    if (currIf) {
                       this.fetchCount++;
                       inRate.current = (inBytes - currIf.traffic.
                           ingress.bytes)/(this.settings.fetchInterval.val);
                       outRate.current = (outBytes - currIf.traffic.
                           egress.bytes)/(this.settings.fetchInterval.val);

                       if (inRate.current < 0) inRate.current = 0;
                       if (outRate.current < 0) outRate.current = 0;
                       if (inRate.current > currIf.traffic.ingress.rate.peak)
                           inRate.peak = inRate.current;
                       else
                           inRate.peak = currIf.traffic.ingress.rate.peak;
                       if (outRate.current > currIf.traffic.egress.rate.peak)
                           outRate.peak = outRate.current;
                       else
                           outRate.peak = currIf.traffic.egress.rate.peak;
                       if (inRate.current > 0) {
                           if (currIf.traffic.ingress.rate.average == 0)
                               this.fetchCount = 0;
                           inRate.average = getAverageRate(currIf.traffic.
                                   ingress.rate.average, inRate.current, 
                                   this.fetchCount);
                       }
                       if (outRate.current > 0) { 
                           if (currIf.traffic.egress.rate.average == 0)
                               this.fetchCount = 0;
                           outRate.average = getAverageRate(currIf.traffic.
                                   egress.rate.average, outRate.current, 
                                   this.fetchCount);
                       }
                    }
                    return { 
                        ingress : {
                            bytes : inBytes,
                            packets : traffic.get('input-packets'),
                            rate : inRate
                        },
                        egress : {
                            bytes : outBytes,
                            packets : traffic.get('output-packets'),
                            rate : outRate
                        }
                    };
                }
                return null;
            },
            /*
             * Things to do when a new interface is selected.
             */
            onInterfaceChange : function (interfaceName) {
                /*
                 * We set this flag here to let an existing fetchInterfaceStats
                 * routine know that the interface selection has changed and
                 * it needs to return. We start a new loop with the new 
                 * interface below.
                 */
                this.hasInterfaceChanged = true;
                this.updateView({
                    appContent : {
                        ifdList : this.ifdList, 
                        loadingInfo : true,
                        interface : null,
                        stats : []
                    }
                });
                this.chart.reset();
                /* 
                 * reset the state 
                 */
                for (prop in this.currInterface) {
                    if (this.currInterface.hasOwnProperty(prop)) {
                        this.currInterface[prop] = null;
                    }
                }
                this.fetchInterfaceStats(interfaceName);
                this.hasInterfaceChanged = false;
            },
            updateView : function (content) {
                this.view.set('controller.op', content);
            },
            /*
             * Pick up saved settings from local storage if present else create
             * new ones and return them to the app. The settings object is
             * saved as a string in the local storage, hence the need to parse 
             * it. If app settings code has changed, we purge the local 
             * storage and update the app settings with defaults.
             */
            initSettings : function () {
                if (localStorage && 'appSettings' in localStorage) {
                    var settings = JSON.parse(localStorage.getItem(
                                'appSettings'));
                    var def = this.defaultSettings;
                    for (p in def) {
                        if (!settings.hasOwnProperty(p) ||
                                def[p].type != settings[p].type) {
                            /* Local storage is stale, purge it */
                            localStorage.removeItem('appSettings');
                            var s = JSON.stringify(def);
                            this.settings = JSON.parse(s);
                            localStorage.setItem('appSettings', s);
                        }
                    }
                    this.settings = settings;
                } else {
                    this.settings = JSON.parse(
                            JSON.stringify(this.defaultSettings));
                }
            },
            /*
             * Load app settings into the settings panel. Cycle through radio 
             * options and set the 'checked' property based on the settings. 
             * For text we simply the value.
             */
            loadSettings : function (divClass, settings) {
                if (settings) {
                    for (prop in settings) {
                        if (settings.hasOwnProperty(prop) && 
                                settings[prop].type == 'radio') {
                            $('.' + divClass + ' input[name=' + prop + 
                                    ']:radio')
                                .each(function() {
                                if ($(this).val() == settings[prop].val) {
                                    $(this).prop('checked', true);
                                }
                            });
                        }
                        if (settings.hasOwnProperty(prop) && 
                                settings[prop].type == 'text') {
                            $('#' + prop).val(settings[prop].val);
                        }
                    }
                }
            },
            /*
             * Save the settings into local storage if present. We need to 
             * stringify the settings since local storage simply stores 
             * key=value pairs, value being a string. We can recover our 
             * settings structure by using JSON.parse on the stringified 
             * settings.
             */
            saveSettings : function (divClass) {
                var settings = this.settings;
                for (prop in settings) {
                   if (settings.hasOwnProperty(prop) && 
                           settings[prop].type == 'radio') {
                       settings[prop].val = $('.' + divClass + ' input[name=' + 
                               prop + ']:checked').val();
                   } 
                   if (settings.hasOwnProperty(prop) && 
                           settings[prop].type == 'text') {
                       settings[prop].val = $('#' + prop).val();
                   }
                }
                if (localStorage) {
                    localStorage.removeItem('appSettings');
                    localStorage.setItem('appSettings', 
                            JSON.stringify(settings)); 
                }
            },
            /*
             * Cycle through the settings and validate them based on min and 
             * max values
             */
            validateSettings : function (divClass) {
                var s = this.settings;
                for (prop in s) {
                    if (s.hasOwnProperty(prop) && s[prop].type == "text") {
                        var input = $('#' + prop).val();
                        if (input < s[prop].min || input > s[prop].max) {
                            alert(prop + " should be between " + s[prop].min +
                                " and " + s[prop].max + " " + s[prop].unit);
                            return false;
                        }
                    }
                }
                return true;
            }
        };
        return monitorInterfaceApp;
    }
    /*
     * Defines application views.
     * @param => appCtx : An application instance/context
     */
    function initAppViews(appCtx) {
        Clira.monitorInterfaceApp = {
            /*
             * View for the Interface selection section
             */ 
            selectInterfaceView : Ember.View.extend({
            /*
             * To make the Selectric plugin work for this app, we need to make
             * sure the elements are inserted into the DOM before we use the
             * Jquery selectors to hook up the plugin. Ember's select helper 
             * generates a lot of <script> metamorphs for data binding which 
             * breaks the plugin. Instead we use the render hook to manually 
             * generate the <option> tags for the select element to ensure 
             * Selectric works. 
             */
                render : function (buffer) {
                    var selected = '';
                    if (appCtx.currInterface.type == 'ifl')
                        selected = ' selected'; 
                    appCtx.ifdList.forEach(function(ifd) {
                        buffer.push("<option value='" + ifd.name + "'>" + 
                            ifd.name + "</option>");
                        ifd.iflList.forEach(function(ifl) {
                            buffer.push("<option value='" + ifl + "'" + 
                                selected + ">" + ifl + "</option>");
                        });
                    });
                },
                /*
                 * This bit is called when the view's elements are inserted 
                 * into the DOM. We do our Selectric plugin setup up here.
                 * optionsItemBuilder is the plugin hook to format the select
                 * box items depending on IFD and IFL.
                 */
                didInsertElement: function() {
                   this.$("#selectInterface").selectric({
                        onChange : function () {
                            appCtx.onInterfaceChange($(this).val()); 
                        },
                        optionsItemBuilder : function (item, element, index) {
                            if (!isIfl(item.text))
                                return "<label class='selectricIfd'>" + 
                                    item.text + "</label>";
                            else
                                return "<label class='selectricIfl'>" + 
                                    item.text + "</label>";
                        }
                    });
                },
            }),
            /*
             * This view is triggered when the settings option is clicked.
             * Creates all the settings panel data and loads the app data
             * stored in the browser local storage. Also assigns the event
             * handlers for various actions like saving app settings,etc.
             */
            appSettingsView : Ember.View.extend ({
                didInsertElement : function () {
                    var settingsPanel = this.$('.appSettingsPanel'),
                        view = this;
                    this.$('#appSettingsBtn').click(function (event) {
                        event.preventDefault();
                        appCtx.loadSettings('settings', appCtx.settings);
                        view.$("#applySettingsBtn").prop('disabled', true);
                        settingsPanel.slideToggle(200);
                    });  
                    this.$(".settings input:radio").change(function () {
                       view.$("#applySettingsBtn, #resetSettingsBtn")
                          .prop('disabled', false);
                    });
                    this.$(".settings input:text").keyup(function () {
                       view.$("#applySettingsBtn, #resetSettingsBtn")
                          .prop('disabled', false);
                    });
                    var reset = false;
                    this.$("#resetSettingsBtn").click(function (event) {
                        event.preventDefault();
                        appCtx.loadSettings('settings', appCtx.defaultSettings);
                        $(this).prop('disabled', true);
                        view.$("#applySettingsBtn").prop('disabled', false);
                        reset = true;
                    });
                    this.$("#applySettingsBtn").click(function (event) {
                        event.preventDefault();
                        if (!appCtx.validateSettings('settings'))
                            return;
                        appCtx.saveSettings('settings');
                        var currIf = appCtx.currInterface;
                        var settings = (reset) ? appCtx.defaultSettings : 
                            appCtx.settings;
                        reset = false;
                        appCtx.chart.applySettings(settings);
                        if (appCtx.currInterface.type == 'ifd') {
                            appCtx.updateView({
                                appContent : {
                                    interface : currIf.ifd,
                                    stats : getStats(currIf.ifd)
                                }
                            });
                        } else {
                            appCtx.updateView({
                                appContent : {
                                    interface : currIf.ifl,
                                    stats : getStats(currIf.ifl)
                                }
                            });
                        }
                        $(this).prop('disabled', true);
                    });
                    this.$("#closeSettingsBtn").click(function(event) {
                        event.preventDefault();
                        settingsPanel.hide(400); 
                    });
                }
            }),
            /*
             * Chart view to initialize the Smoothie chart plugin. 
             */
            chartView : Ember.View.extend({
                didInsertElement: function() {
                    appCtx.chart.init(this.$("#usageGraph"), appCtx.settings);
                    this.$("#usageGraph").hide().slideToggle('slow', 
                        function() {
                            appCtx.chart.smoothie.streamTo($(this)[0], 
                            (appCtx.settings.fetchInterval.val * 1000));
                        });
                    appCtx.chart.applySettings(appCtx.settings);
                }
            }),
            /*
             * Interface information view.
             */
            interfaceInfoView : Ember.View.extend({
                templateName : function () {
                    if (appCtx.currInterface.type == 'ifl') {
                        return 'info-ifl';
                    } else {
                        return 'info-ifd';
                    }               
                }.property(),
                /*
                 * Make the speeds more readable
                 */
                speed : function () {
                    var speed;
                    if (appCtx.currInterface.type == 'ifl') {
                        speed = appCtx.currInterface.ifl.ifdSpeed
                    } else {
                        speed = appCtx.currInterface.ifd.speed;
                    }
                    if (!speed) return "N/A";
                    var gSpeed = speed / 1000000000;
                    if (gSpeed >= 1) return gSpeed + "Gbps";
                    return (speed / 1000000) + "Mbps";
                }.property()
            }),
            /*
             * Interface statistics view. Computes the values based on unit
             * selection
             */
            statisticsView : Ember.View.extend({
                templateName : 'statistics',
                stats : function () {
                    var stats = this.get('content');
                    for (type in stats) {
                        if (!stats.hasOwnProperty(type))continue;
                        var rates = stats[type].rate;
                        for (rate in rates) {
                            if (rates.hasOwnProperty(rate)) {
                                rates[rate] = convertRate(rates[rate].value, 
                                    appCtx.settings.rateUnit.val);
                            }
                        }
                    }
                    return stats;
                }.property('content')
            }),
            /*
             * Fade in effect
             */
            fadeInView : Ember.View.extend({
                didInsertElement: function() {
                    this.$().hide().fadeIn(500);
                } 
            })
        }
    }
    /*
     * Extract traffic and errors from an interface object for each type 
     * (ingress/egress)and populate a stats array which will be consumed by 
     * the statisticsView logic.
     */
    function getStats (_interface) {
        var stats = {};
        if ('traffic' in _interface && _interface.traffic) {
            var traffic = _interface.traffic;
            var errors = _interface.errors || {};
            for (type in traffic) {
                if (traffic.hasOwnProperty(type)) {
                    stats[type] = {
                        bytes : traffic[type].bytes,
                        packets : traffic[type].packets,
                        rate : {
                            current : {
                                value :  traffic[type].rate.current,
                                unit : null
                            },
                            peak : {
                                value : traffic[type].rate.peak,
                                unit : null
                            },
                            average : {
                                value : traffic[type].rate.average,
                                unit : null
                            }
                        },
                        error : errors[type]
                    };
                }
            }
        }
        return stats;
    }
    /*
     * Convert interface speed strings (Mbps/Gbps) into their number eqv.
     */
    function strToSpeed(speedStr) {
        if (speedStr) {
            if ((/mbps/i).test(speedStr)) {
                return parseInt(speedStr) * 1000000;
            } else if ((/gbps/i).test(speedStr)) {
                return parseInt(speedStr) * 1000000000;
            }
        }
        return 0;
    }
    /*
     * Get the parent IFD interface speed from the IFD list for a given ifl.
     * Return 0 if speed is not available.
     */
    function getIfdSpeed(ifdList, ifl) {
        for (var i = 0; i < ifdList.length; i++) {
            if (ifdList[i].name == getIfdFromIfl(ifl)) {
                return ifdList[i].speed;
            }
        }
        return 0;
    }
    /*
     * Convert a traffic rate value to the specified unit. In case of Auto,
     * we keep dividing the original value by 1024 until we have an 
     * integral value, which we return.
     */
    function convertRate(value, unit) {
        var divider = 1,
            units = ['bps', 'Kbps', 'Mbps', 'Gbps'];
        if (value) {
            if (unit == 'Kbps')divider = 1024;
            else if (unit == 'Mbps')divider = 1024 * 1024;
            else if (unit == 'Gbps')divider = 1024 * 1024 * 1024;
            else if (unit == 'Auto') {
               /*
                * Convert to bits
                */
               value *= 8;
               for (var i=0; i < units.length ; i++) {
                   var newVal = value / 1024;
                   if (newVal < 1) {
                       return {
                           value : parseFloat(value.toFixed(2)),
                           unit : units[i]
                       };
                   }
                   value = newVal;
               } 
               return {
                   value : parseFloat(value.toFixed(2)),
                   unit : 'Gbps'
               };
            }
            /* For unit != 'Auto' */
            return {
                value : parseFloat((value * 8 / divider).toFixed(2)),
                unit : unit
            }
        } 
        /* Null values */
        return {value : 0, unit : null};
    }
    /*
     * Status tells us if CLIRA was able to send the command over to the device
     * We check the reply for an 'error-message' element to confirm a command 
     * failure on the device.
     */
    function isError (status, res, ctx) {
        var err;
        if (!status) {
            ctx.appError = "Error : Fatal RPC error";
            return true;
        }
        if (err = res.get('error-message')) {
           ctx.appError = "Error : " + err;
           return true;
        }
        return false;
    }
    /*
     * Calculate continous average.
     */
    function getAverageRate (oldAvg, newValue, sampleSz) {
        return parseFloat(((oldAvg * sampleSz + newValue)/
                (sampleSz + 1)).toFixed(2));
    }
    /*
     * Return the iflList from an IFD
     */
    function getIflList(ifd) {
        var ifls = [],
            i = ifd.get('logical-interface', false);
        if (i) {
            i.each(function (ifl) {
                ifls.push(ifl.get('name'));
            });
        }
        return ifls;
    }
    /*
     * Return primary IP address for an address family
     */
    function getIflAddresses(family) {
        var addrs = [],
            i = family.get('interface-address', false);
        if (i) {
            i.each(function (addr) {
                addrs.push(addr.get('ifa-local'));
            });
        }
        return addrs[0];
    }
    function getIfdFromIfl(ifl) {
        var idx = ifl.indexOf('.');
        if (idx == -1) return ifl;
        else return ifl.substr(0, idx);
    }
    function isIfl (name) {
        if (name.indexOf(".") == -1) return false;
        return true;
    }
    /*
     * Create the Smoothie chart instance. For various options please refer:
     * http://smoothiecharts.org/
     * Also define various chart methods. The contructor takes in the canvas
     * element object as argument in which the chart is drawn.
     */
    function RealTimeChart() {
        var smoothieOptions = {
            grid: { 
                lineWidth: 1, 
                fillStyle:'rgba(254,249,206,0.93)',
                millisPerLine: 10000, 
                verticalSections: 5
            }, 
            labels: {
                 fillStyle:'#636263',
                 fontSize:9,
                 precision:0
            },
            minValue : 0,
            millisPerPixel : 200,
            timestampFormatter:SmoothieChart.timeFormatter
        },
        ingressOptions = {
            lineWidth:2,
            strokeStyle:'#fd8633',
            fillStyle:'rgba(211,66,1,0.30)'    
        },
        egressOptions = {
            lineWidth:2,
            strokeStyle:'#548dfb',
            fillStyle:'rgba(32,47,174,0.30)'
        },
        smoothie = new SmoothieChart(smoothieOptions),
        ingress = new TimeSeries(),
        egress = new TimeSeries();
        smoothie.addTimeSeries(ingress, ingressOptions);
        smoothie.addTimeSeries(egress, egressOptions);
        this.canvas = null;
        this.smoothie = smoothie;
        this.series = { ingress : ingress, egress : egress }; 
    }
    RealTimeChart.prototype = {
        init : function (canvas, settings) {
            this.canvas = canvas;
            this.updateSize(settings);
        },
        plotUsage : function (_interface) {
            if (_interface.traffic) {
                var speed = 0, 
                    traffic = _interface.traffic;
                if (_interface.hasOwnProperty('speed'))
                    speed = _interface.speed;
                else
                    speed = _interface.ifdSpeed;
                if (speed) {
                    var ingress = ((parseInt(traffic.ingress.rate.current) 
                                * 8) / speed) * 100;
                    var egress = ((parseInt(traffic.egress.rate.current) 
                                * 8) / speed) * 100;
                    this.series.ingress.append(new Date().getTime(),
                        ingress);
                    this.series.egress.append(new Date().getTime(), 
                        egress);
                }
            }
        },
        applySettings : function (settings) {
            var s = settings,
                period = s.chartPeriod.val,
                numLines = s.chartNumLines.val,
                width = s.chartWidth.val,
                autoscale = (s.autoScale.val == 'yes') ? true : false;
            this.updateSize(settings);
            this.updatePeriod(period, width);
            this.updateLineInterval(numLines, width);
            if (autoscale) {
                this.updateRange(0, undefined);
            } else {
                this.updateRange(0, 100);
            }
        },
        updatePeriod : function (period, width) {
            var msPerPx = parseInt((1000 * period) / width);
            this.smoothie.options.millisPerPixel = msPerPx;
        },
        updateLineInterval : function (numLines, width) {
            var msPerPx = this.smoothie.options.millisPerPixel,
                msPerLine = parseInt((msPerPx * width) / numLines);
            this.smoothie.options.grid.millisPerLine = msPerLine;
        },
        updateRange : function (min, max) {
            this.smoothie.options.minValue = min;
            this.smoothie.options.maxValue = max;
            this.smoothie.updateValueRange();        
        },
        updateSize: function (settings) {
            var width = settings.chartWidth.val;
            var height = settings.chartHeight.val;
            if (width && this.canvas.prop('width') != width) 
                this.canvas.prop('width', width);
            if (height && this.canvas.prop('height') != height) 
                this.canvas.prop('height', height);
        },
        reset : function () {
            this.series.ingress.data = [];
            this.series.egress.data = [];        
        }
    }
    /*
     * Function to parse the json rpc-reply.
     */
    function parseRPC(rpcReply) {
        if (!rpcReply)return null;
        return new RPCReply(JSON.parse(rpcReply));
    }
    function RPCReply (res) {
        this.data = res;
    }
    RPCReply.prototype = {
        /*
         * A wrapper around findProp, searches for a property inside a 
         * rpc-reply json object. If araryToObj flag is true and if the 
         * found property refers to an array of size 1, the object inside it
         * will be returned. This is just for convenience, to get around the 
         * single instance container output which is always wrapped around an
         * array.
         *
         * For eg if res is an object containting the
         * below json response :
         *
         *    "physical-interface" : [
         *   {
         *       "name" : [
         *       {
         *           "data" : "cbp0"
         *       }
         *       ], 
         *   }
         *   res.get('name') => 'cbp0'
         *   res.get('physical-interface') => res['physical-interface'][0]
         *   res.get('physical-interface', false) => res['physical-interface'] 
         */
        get : function (prop, arrayToObj) {
            var obj = this.findProp(this.data, prop, null);
            if (!obj) return null;
            if (typeof(arrayToObj) === 'undefined')arrayToObj = true;
            if (obj.hasOwnProperty('length') && obj.length == 1) {
                if ('data' in obj[0]) return obj[0].data;
                else if (arrayToObj)return new RPCReply(obj[0]);
            }
            return new RPCReply(obj);
        },
        /*
         * Recursively search the json object. 
         */
        findProp : function (obj, propToFind) {
            var foundObj = null;
            if (!obj) return null;
            if (typeof obj != 'object') {
                return null;
            }
            if (obj.hasOwnProperty(propToFind)) return obj[propToFind];
            for (prop in obj) {
                if (obj.hasOwnProperty(prop) && obj[prop] 
                        && obj[prop].constructor == 'Array') {
                    for (i=0 ; i < obj[prop].length ; i++) {
                        foundObj = this.findProp(obj[prop][i], propToFind);
                        if (foundObj)return foundObj;
                    }
                } else {
                    foundObj = this.findProp(obj[prop], propToFind);
                    if (foundObj) return foundObj;
                }
            }
            return foundObj;
        },
        /*
         * Iterate over an rpc-reply type object or array.
         */
        each : function (callback) {
            if (this.isArray()) {
                for (var i=0; i < this.data.length; i++) {
                    callback(new RPCReply(this.data[i]), i);
                }
            } else if (typeof this.data == 'object') {
                for (prop in this.data) {
                    if (this.data.hasOwnProperty(prop)) {
                        callback(new RPCReply(this.data[prop]), prop);
                    }
                }
            } else {
                console.error(this.constructor + 
                        ' passed, instead of object or array.');
            }
        },
        /*
         * Test whether current object is an array
         */
        isArray : function () {
            return (Object.prototype.toString.call(this.data) === 
                    '[object Array]');
        }
    }
});
