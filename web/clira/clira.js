/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2011, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    var $output = $("#output-top");
    var muxer;
    var command_number = 0;
    var tgtHistory;
    var renderBuffer = {};

    var loadingMessage = "<img src='/images/icons/loading.png'>"
        + "    ....loading...\n";

    if ($.clira == undefined)
        $.clira = { };

    $.extend($.clira, {
        decorateIcons: function decorateIcons ($wrapper) {
            // Our container decorations (which need some of our functions)
            $(".icon-remove-section", $wrapper).text("Close").button({
                text: false,
                icons: { primary: "ui-icon-closethick" }
            }).click(function () {
                divRemove($wrapper);
            });

            $(".icon-hide-section", $wrapper).text("Hide").button({
                text: false,
                icons: { primary: "ui-icon-minusthick" }
            }).click(function () {
                divHide($wrapper);
            });

            $(".icon-unhide-section", $wrapper).text("Unhide").button({
                text: false,
                icons: { primary: "ui-icon-plusthick" }
            }).click(function () {
                divUnhide($wrapper);
            }).addClass("hidden");

            $(".icon-clear-section", $wrapper).text("Clear").button({
                text: false,
                icons: { primary: "ui-icon-trash" }
            }).click(function () {
                $(".can-hide", $wrapper).text("");
            });

            $(".icon-keeper-section", $wrapper).text("Keep").button({
                text: false,
                icons: { primary: "ui-icon-star" }
            }).click(function () {
                $wrapper.toggleClass("keeper-active");
                $(this).toggleClass("ui-state-highlight");
            });
        },

        makeAlert: function makeAlert (view, message, defmessage) {
            if (message == undefined || message.length == 0)
                message = defmessage;
            var content = '<div class="ui-state-error ui-corner-all">'
                + '<span><span class="ui-icon ui-icon-alert">'
                + '</span>';
            content += '<strong>Error:</strong> ' + message + '</span></div>';
            view.get('controller').set('output', content);
        },

        refocus: function () {
            if (tgtHistory.value())
                $.clira.cmdHistory.focus();
            else tgtHistory.focus();
        },

        /*
         * Executes given command and inserts output container into the page
         */
        executeCommand: function executeCommand (command, content) {
            var parse = $.clira.parse(command);

            if (parse.possibilities.length > 0) {
                var poss = parse.possibilities[0];
                content.contentTemplate = poss.command.templateName;
                content.poss = poss;            

                if (content.commandNumber == undefined) {
                    if ($.clira.commandCount) {
                        content.commandNumber = ++$.clira.commandCount;
                    } else {
                        content.commandNumber = 1;
                    }
                }
                
                Clira.__container__.lookup('controller:outputs')
                     .unshiftObject(Clira.OutputContainerController
                                         .create(content));
            }
        }
    });

    $.dbgpr("document is ready");

    var testForms = {
        first: {
            title: "Ping Options",
            command: {
                rpc: "ping"
            },
            tabs: { },
            css: "/css/ping.css",
            fieldsets: [
                {
                    legend: "Basic",
                    fields: [
                        {
                            name: "host",
                            title: "Hostname or IP address of remote host",
                            type: "text"
                        },
                        {
                            name: "count",
                            title: "Number of ping requests to send",
                            type: "number",
                            range: "1..2000000000",
                            units: "packets"
                        },
                        {
                            name: "interval",
                            title: "Delay between ping requests",
                            units: "seconds",
                            type: "number"
                        },
                        {
                            name: "no-resolve",
                            title: "Don't attempt to print addresses symbolically",
                            type: "boolean"
                        },
                        {
                            name: "size",
                            title: "Size of request packets",
                            type: "number",
                            units: "bytes",
                            range: "0..65468"
                        },
                        {
                            name: "wait",
                            title: "Delay after sending last packet",
                            type: "number",
                            units: "seconds"
                        }
                    ]
                },
                {
                    legend: "Outgoing",
                    fields: [
                        {
                            name: "bypass-routing",
                            title: "Bypass routing table, use specified interface",
                            type: "boolean"
                        },
                        {
                            name: "do-not-fragment",
                            title: "Don't fragment echo request packets (IPv4)",
                            type: "boolean"
                        },
                        {
                            name: "inet",
                            title: "Force ping to IPv4 destination",
                            type: "boolean"
                        },
                        {
                            name: "inet6",
                            title: "Force ping to IPv6 destination",
                            type: "boolean"
                        },
                        {
                            name: "logical-system",
                            title: "Name of logical system",
                            type: "string"
                        },
                        {
                            name: "interface",
                            title: "Source interface (multicast, all-ones, unrouted packets)",
                            type: "interface-name"
                        },
                        {
                            name: "routing-instance",
                            title: "Routing instance for ping attempt",
                            type: "string"
                        },
                        {
                            name: "source",
                            title: "Source address of echo request",
                            type: "ip-address"
                        }
                    ]
                },
                {
                    legend: "ICMP",
                    fields: [
                        {
                            name: "loose-source",
                            title: "Intermediate loose source route entry (IPv4)",
                            type: "boolean"
                        },
                        {
                            name: "record-route",
                            title: "Record and report packet's path (IPv4)",
                            type: "boolean"
                        },
                        {
                            name: "strict",
                            title: "Use strict source route option (IPv4)",
                            type: "boolean"
                        },
                        {
                            name: "strict-source",
                            title: "Intermediate strict source route entry (IPv4)",
                            type: "boolean"
                        }
                    ]
                },
                {
                    legend: "Advanced",
                    fields: [
                        {
                            name: "detail",
                            title: "Display incoming interface of received packet",
                            type: "boolean"
                        },
                        {
                            name: "verbose",
                            title: "Display detailed output",
                            type: "boolean"
                        },
                        {
                            name: "mac-address",
                            title: "MAC address of the nexthop in xx:xx:xx:xx:xx:xx format",
                            type: "mac-address"
                        },
                        {
                            name: "pattern",
                            title: "Hexadecimal fill pattern",
                            type: "string"
                        },
                        {
                            name: "rapid",
                            title: "Send requests rapidly (default count of 5)",
                            type: "boolean"
                        },
                        {
                            name: "tos",
                            title: "IP type-of-service value",
                            type: "number",
                            range: "0..255"
                        },
                        {
                            name: "ttl",
                            title: "IP time-to-live value (IPv6 hop-limit value)",
                            type: "number",
                            units: "hops",
                            range: "0..63"
                        }

                    ]
                }
            ]
        }
    }

    function parseParams (cmdline) {
        var argv = cmdline.split(" ");
        var params = { };
        for (var i = 0; i < argv.length; i += 2) {
            params[argv[i]] = argv[i + 1];
        }
        return params;
    }

    function openMuxer () {
        if (muxer) {
            return;
        }

        muxer = $.Muxer({
            url: $.clira.prefs.mixer,
            onopen: function (event) {
                $.dbgpr("clira: WebSocket has opened");
            },
            onreply: function (event, data) {
                $.dbgpr("clira: onreply: " + data);
            },
            oncomplete: function (event) {
                $.dbgpr("clira: complete");
            },
            onclose: function (event) {
                $.dbgpr("clira: WebSocket has closed");
                // This muxer has been closed.  Completely nuke it since we
                // only hold one muxer in memory per browser instance.
                muxer = undefined;
            },
            onhostkey: function (view, data) {
                var self = this;
                promptForHostKey(view, data, function (response) {
                    muxer.hostkey(self, response, data);
                });
            },
            onpsphrase: function (view, data) {
                var self = this;
                promptForSecret(view, data, function (response) {
                        muxer.psphrase(self, response, data);
                }, function() {
                    $.clira.makeAlert(view,
                        "Incorrect or unspecified passphrase");
                });
            },
            onpsword: function (view, data) {
                var self = this;
                promptForSecret(view, data, function (response) {
                    muxer.psword(self, response, data);
                }, function() {
                    $.clira.makeAlert(view, 
                        "Incorrect or unspecified password");
                });
            }
        });

        muxer.open();
    }

    function restoreReplaceDiv ($div) {
        $.clira.cmdHistory.focus();
        $div.parent().html(loadingMessage);
        $div.remove();
    }

    $.extend($.clira, {
        muxer: function () {
            if (muxer == undefined || muxer.failed) {
                openMuxer();
            }
            return muxer;
        },
        runCommand: function runCommand (view, target, command, onComplete) {
            this.runCommandInternal(view, target, command, onComplete, null);
        },        
        /*
         * runCommandInternal takes additional parameters that dictates how to
         * run the command. 'format' is the format in which RPC should request
         * output in and defaults to html. 'stream' is boolean when set will
         * send RPC with stream flag and receives streaming RPC output.
         * 'onReplyRender' when specified will be called with data received
         * per reply. 'onCompleteRender' will be run once the RPC is completed
         */
        runCommandInternal: function runCommandInternal (view, target, command,
                                                         onComplete, format, 
                                                         stream, onReplyRender,
                                                         onCompleteRender) {
            var output = null,
                domElement = false;
            
            /*
             * If runCommand is called without a view, create a pseudo view
             * and use for pop ups that need user input
             */
            if (view instanceof jQuery) {
                output = view;
                domElement = true;
                var pseudoView = Ember.View.extend({
                    init: function() {
                        this._super();
                        this.set('controller', Ember.Controller.create());
                    }
                });
                view = Ember.View.views["pseudo_view"]
                            .createChildView(pseudoView);
                view.appendTo(output);
            } else {
                output = view.$();
            }

            if (domElement) {
                output.html(loadingMessage);
            } else {
                view.get('controller').set('output', loadingMessage);
            }

            if (muxer == undefined)
                openMuxer();

            // Set muxer on the controller
            view.set('controller.muxer', muxer);

            var full = [ ];
            var payload = "<command format=";

            if (format) {
                payload += "'" + format + "'";
            } else {
                payload += "'html'";
            }

            if (stream) {
                payload += " stream='stream'";
            }

            payload += ">" + command + "</command>";
            var op = '',
                cache = [];
            muxer.rpc({
                div: output,
                view: view,
                target: target,
                payload: payload,
                onreply: function (data) {
                    $.dbgpr("rpc: reply: full.length " + full.length
                            + ", data.length " + data.length);
                    full.push(data);

                    // Turns out that if we continually pass on incoming
                    // data, firefox becomes overwhelmed with the work
                    // of rendering it into html.  We cheat here by
                    // rendering the first piece, and then letting the
                    // rest wait until the RPC is complete.  Ideally, there
                    // should also be a timer to render what we've got if
                    // the output RPC stalls.
                    if (stream) {
                        cache.push(data);
                        if (cache.length > 5) {
                            cache = cache.splice(cache.length - 5);
                        }
                        op = cache.join("");
                    } else {
                        op = full.join("");
                    }

                    if (stream || full.length <= 2) {
                        if (domElement) {
                            output.html(data);
                        } else {
                            if (onReplyRender) {
                                view.get('controller')
                                    .set('output',
                                         onReplyRender(op));
                            } else {
                                view.get('controller')
                                    .set('output', op);
                            }
                        }
                    }
                },
                oncomplete: function () {
                    $.dbgpr("rpc: complete");
                    if (domElement) {
                        output.html(full.join(""));
                    } else {
                        view.get('controller').set('completed', true);
                        if (onCompleteRender) {
                            view.get('controller')
                                .set('output', 
                                     onCompleteRender(full.join("")));
                        } else {
                            view.get('controller')
                                .set('output', full.join(""));
                        }
                    }

                    // Add this device to list of recently used devices
                    view.get('controller').get('controllers.recentDevices')
                                          .addDevice(target);
                    if ($.isFunction(onComplete)) {
                        onComplete(true, output);
                    }
                },
                onclose: function (event, message) {
                    if (stream) {
                        this.oncomplete();
                    }
                    $.dbgpr("muxer: rpc onclose");
                    if (full.length == 0) {
                        $.clira.makeAlert(view, message,
                                          "internal failure (websocket)");
                        if ($.isFunction(onComplete)) {
                            onComplete(false, output);
                        }
                    }
                },
                onerror: function (message) {
                    $.dbgpr("muxer: rpc onerror");
                    if (full.length == 0) {
                        $.clira.makeAlert(view, message,
                                          "internal failure (websocket)");
                        if ($.isFunction(onComplete)) {
                            onComplete(false, output);
                        }
                    }
                }
            });
        },

        runSlax: function (options) {
            if (!muxer) {
                openMuxer();
            }

            muxer.slax({
                script: options.script,
                view: options.view,
                type: options.type,
                args: options.args,
                oncomplete: function (data) {
                    if (options.success) {
                        options.success(data);
                    }
                },
                onerror: function (data) {
                    if (options.failure) {
                        options.failure(data)
                    }
                }
            });
        },

        prefsChangeMuxer: function prefsChangeMuxer (value, initial, prev) {
            if (initial)
                return;

            if (muxer) {
                muxer.close();
                muxer = undefined;
            }
        },

        commandOutputTrimChanged: function commandOutputTrimChanged () {
            $.clira.commandOutputTrim(0);
        },

        commandOutputTrim: function commandOutputTrim (fresh_count) {
            var last = $output.get(0);
            if (!last)
                return;

            var keep = 0;
            for (var i = 0; last.children[i]; i++) {
                var $child = $(last.children[i]);
                if ($child.hasClass("keeper-active")) {
                    keep += 1;
                } else if (i < fresh_count) {
                    // do nothing
                } else if (i >= $.clira.prefs.output_remove_after + keep) {
                    divRemove($child);
                } else if (i >= $.clira.prefs.output_close_after + keep) {
                    if (!divIsHidden($child))
                        divHide($child);
                } else {
                    if (divIsHidden($child))
                        divUnhide($child);
                }
            }
        },

        targetListMarkUsed: function targetListMarkUsed (target, cname,
                                                        callback) {
            var $tset = $("#target-contents");
            var id = "target-is-" + cname;
            var $target = $("#" + id, $tset);

            if ($target && $target.length > 0) {
                $target.remove();
                $tset.prepend($target);

            } else {

                var content = "<div id='" + id + "' class='target " + id 
                    + " target-info rounded buttonish green'>"
                    + target
                    + "</div>";

                $target = jQuery(content);
                $tset.prepend($target);
                $(".target", $tset).each(function (index, target) {
                    var delta = $(target).position().top -
                        $(target).parent().position().top;
                    if ( delta > $.clira.prefs.max_target_position) {
                        $(target).remove();
                    }
                });     
            }

            $target.click(function (event) {
                if (tgtHistory) {
                    tgtHistory.close();
                    tgtHistory.select(target);
                }
                if (callback)
                    callback($target, target);
                $.clira.cmdHistory.focus();
            });

            $("#target-contents-none").css({ display: "none" });
        }
    });

    function promptForHostKey (view, data, onclick) {
        var hostKeyView = Clira.DynFormView.extend({
            title: "Host key for " + data.target,
            buttons: [{
                caption: "Accept",
                onclick: function() {
                    var onclick = this.get('parentView.parentView.onclick');
                    onclick("yes");
                    this.get('parentView.parentView').destroy();
                }
            },{
                caption: "Decline",
                onclick: function() {
                    var onclick = this.get('parentView.parentView.onclick');
                    onclick("no");
                    this.get('parentView.parentView').destroy();
                }
            }]
        }); 

        /*
         * Register hostKeyView, get an instance and push it to the container
         */
        view.get('parentView').container.register('view:hostKey', hostKeyView);
        var hkv = view.get('parentView').container.lookup('view:hostKey');
        hkv.message = data.prompt.split(/(?:\n)+/); 
        hkv.onclick = onclick;
        view.get('parentView').pushObject(hkv);
    }

    function promptForSecret (view, data, onclick, onclose) {
        var secretView = Clira.DynFormView.extend({
            title: data.target,
            buttons: [{
                caption: "Enter",
                onclick: function() {
                    var onclick = this.get('parentView.parentView.onclick');
                    onclick(this.get('controller.fieldValues').password);
                    this.$().context.enter = true;
                    this.get('parentView.parentView').destroy();
                }
            },{
                caption: "Cancel",
                onclick: function() {
                    if (!this.$().context.enter) {
                        this.get('parentView.parentView.onclose')();
                    }
                    this.get('parentView.parentView').destroy();
                }
            }]
        });
        var fields = [{
            name: "password",
            title: "",
            secret: true
        }];

        /*
         * Register secretView, get an instance and push it to the container
         */
        view.get('parentView').container.register('view:secret', secretView);
        var sv = view.get('parentView').container.lookup('view:secret');
        sv.fields = fields;
        sv.onclick = onclick;
        sv.onclose = onclose;
        sv.message = data.prompt.split(/(?:\n)+/);
        view.get('parentView').pushObject(sv);
    }

    function loadHttpReply (text, status, http, $this, $output) {
        $.dbgpr("loadHttpReply: ", "target:", target,
                "; status:", status, "; text:", text);
        $.dbgpr("http", http);

        if (status == "error") {
            $this.html("<div class='command-error'>"
                         + "An error occurred: "
                         + http.statusText
                         + "</div>");
        } else {
            var $xml = $($.parseXML(text));
            var $err = $xml.find("[nodeName='xnm:error']");
            if ($err[0])
                $this.html("<div class='command-error'>"
                             + "An error occurred: "
                           + htmlEscape($("message", $err).text())
                             + "</div>");
        }
        $output.slideDown($.clira.prefs.slide_speed);
    }

    function htmlEscape (val) {
        return val.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;");
    }

    function divRemove ($wrapper) {
        $wrapper.slideUp($.clira.prefs.slide_speed, function () {
            $wrapper.remove();
        });
    }

    function divIsHidden ($wrapper) {
        return $(".icon-hide-section", $wrapper).hasClass("hidden");
    }

    function divHide ($wrapper) {
        $(".icon-unhide-section", $wrapper).removeClass("hidden");
        $(".icon-hide-section", $wrapper).addClass("hidden");
        $(".can-hide", $wrapper).slideUp($.clira.prefs.slide_speed);
    }

    function divUnhide ($wrapper) {
        $(".icon-unhide-section", $wrapper).addClass("hidden");
        $(".icon-hide-section", $wrapper).removeClass("hidden");
        $(".can-hide", $wrapper).slideDown($.clira.prefs.slide_speed);
    }

    function hideCommandForm (yform) {
        var $top = $(yform.form).parents("div.output-wrapper");
        var $bar = $("div.output-header", $top);

        if ($("button.icon-show-form", $bar).length == 0) {
            $bar.append("<button class='icon-show-form'/>"
                        + "<button class='icon-hide-form'/>");

            $(".icon-show-form", $bar).text("Show Form").button({
                text: false,
                icons: { primary: "ui-icon-circle-arrow-s" }
            }).click(function () {
                $(".icon-hide-form", $bar).removeClass("hidden");
                $(".icon-show-form", $bar).addClass("hidden");
                yform.form.slideDown($.clira.prefs.slide_speed);
            });

            $(".icon-hide-form", $bar).text("Hide Form").button({
                text: false,
                icons: { primary: "ui-icon-circle-arrow-n" }
            }).addClass("hidden").click(function () {
                $(".icon-show-form", $bar).removeClass("hidden");
                $(".icon-hide-form", $bar).addClass("hidden");
                yform.form.slideUp($.clira.prefs.slide_speed);
            });
        }

        $(".icon-show-form", $bar).removeClass("hidden");
        $(".icon-hide-form", $bar).addClass("hidden");
        yform.form.slideUp($.clira.prefs.slide_speed);
    }

    function runRpc (yform, $output, rpc, target) {
        $.dbgpr("runrpc:", rpc);
        
        $output.slideUp(0).slideDown($.clira.prefs.slide_speed);
        $output.load("/clira/clira.slax", 
            {
                target: target, // target is optional
                rpc: rpc,       // rpc is in string form
                form: "false"   // don't want form in reply
            }, function (text, status, http) {
                loadHttpReply(text, status, http, $(this), $output);
            }
        );
    }
});
