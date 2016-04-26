/*
 * CLIRA application to fetch MPLS LSP info from a JUNOS device and plot the LSP on map 
*/

$(function($) {
    var currentView = null;
    $.clira.commandFile({
        name: "map-mpls-lsp",
        prereqs: [
            "/external/vis/vis.min.js"
        ],
        commands: [{
            command: "map mpls lsp",
            help: "Map lsp for a device",
            templateFile: "/apps/map-mpls-lsp/map-mpls-lsp.hbs",
            arguments: [{
                name: "device",
                help: "Remote JUNOS device",
                type: "string",
                nokeyword: true
            }],
            execute: function(view, cmd, parse, poss) {
                view.set('controller.loading', true);
                currentView = view;
                fetchLSPInfo(view, poss.data.device);

            }
        }]
    });
    
    /*
     * Initialize lookUpMap: It will hold the device ip, device name
     * in CLIRA and the configured device name
    */
    var lookUpMap = {

    };
    
    /*
     * ShowMPLSlspTopology view logic to populate LSP on a graph
     * The graph is generated using the vis.js library.
    */
    var network = null;
    var graph = null;
    Clira.ShowMPLSlspTopology = Ember.View.extend({
        tagName: 'div',
        attributeBindings: ['style'],
        style: null,

        didInsertElement: function() {
            this._super();
            var context = this.get('context');
            this.set('style', "position: relative;");
            this.set('margin-left', 'auto;');
            this.set('margin-right', 'auto;');
            this.set('width', '100%;');
            this.set('height', '100%;');
            $("#inputPackets").empty();
            $("#outputPackets").empty();

            //check if context exists
            if (context) {
                //console.log("Show mpls lsp ShowMPLSlspTopology");
                //initalize the nodesArray and edgesArray
                var nodesArray = [];
                var edgesArray = [];
                for (i = 0; i < context.path.length; i++) {
                    nodesArray.push({
                        id: i + 1,
                        label: lookUpMap[context.path[i]][1]
                    });

                    edgesArray.push({
                        from: i + 1,
                        to: i + 2
                    });
                }

                var nodes = new vis.DataSet(nodesArray);

                // create an array with edges usin vis.js api
                var edges = new vis.DataSet(edgesArray);

                // create a dynamic container to hold the graph
                var container = $("#" + this.get('elementId')).get(0);

                // provide the data in the vis format
                var data = {
                    nodes: nodes,
                    edges: edges
                };

                //provide the options
                var options = {
                    autoResize: true,
                    height: '100%',
                    width: '100%',
                    edges: {
                        arrows: {
                            to: {
                                enabled: true,
                                scaleFactor: 1
                            }
                        }
                    },
                    nodes: {
                        color: '#005FBF',
                        fixed: false,
                        font: '14px arial white',
                        scaling: {
                            label: true
                        },
                        shadow: true
                    }
                };

                // initialize the network object
                network = new vis.Network(container, data, options);

                //add event listener on node click
                network.on("selectNode", function(obj) {
                    //empty the divs on view
                    $("#routerInfo").empty();
                    $("#inputPackets").empty();
                    $("#outputPackets").empty();
                    $("#inputBytes").empty();
                    $("#outputBytes").empty();

                    //get device name from lookUpMap
                    var device = lookUpMap[context.path[obj.nodes[0] - 1]][0];
                    currentView.set('controller.dialogLoading', true);
                    
                    //call getInterfaceStats at 3 sec interval to getch interface statistics
                    var updateDialog = setInterval(function() {
                        getInterfaceStats(currentView, device, lookUpMap[context.path[obj.nodes[0] - 1]][2]);
                    }, 3000);

                    //populate the dialog box on html page
                    $("#dialog").dialog({
                        closeOnEscape: false,
                        open: function(event, ui) {
                            $(".ui-dialog-titlebar-close").hide();
                        },
                        modal: true,
                        draggable: false,
                        resizable: false,
                        width: 400,
                        dialogClass: 'ui-popup',
                        buttons: [{
                            text: "Close",
                            icons: {
                                primary: "ui-icon-closethick"
                            },
                            click: function() {
                                clearInterval(updateDialog);
                                $("#routerInfo").empty();
                                $("#inputPackets").empty();
                                $("#outputPackets").empty();
                                $("#inputBytes").empty();
                                $("#outputBytes").empty();
                                $("#inputPacketsTitle").remove();
                                $("#outputPacketsTitle").remove();
                                $("#inputBytesTitle").remove();
                                $("#outputBytesTitle").remove();
                                currentView.set('controller.dialogLoading', true);
                                $(this).dialog("close");
                            }
                        }]
                    });


                });
            }
        }
    });

    /*
     * The fetchLSPInfo calls the 'show mpls lsp detail' command
     * and populates lspDict json object with needed info.
    */

    function fetchLSPInfo(view, device) {
        ////console.log("In fetchLSPInfo...");
        var cmd = "show mpls lsp detail";
        var lspDict = {};
        lspDict.ingress = [];
        lspDict.egress = [];
        lspDict.transit = [];
        var lspRow = {};
        lspRow.path = [];
        lspRow.locationInfo = [];
        var session_type = "";
        lspRow.myhost = "";
        var resultStatus = false;
        //call show mpls lsp detail command
        $.clira.runCommand(view, device, cmd, "xml", function(view, status,
            result) {
            if (status) {
                //console.log("In runCommand Callback");
                //console.log(result);
                //iterate over the result xml object and parse
                $(result).find("rsvp-session-data").each(function() {
                    //console.log("rsvp-session-data");
                    resultStatus = true;
                    session_type = $(this).find("session-type").text();
                    //console.log(session_type);
                    $(this).find("rsvp-session").each(function() {
                        //console.log("rsvp-session");
                        //check if session_type is 'Ingress' and populate Ingress JSON
                        if (session_type === "Ingress") {
                            $(this).find("mpls-lsp").each(function() {
                                //console.log("mpls-lsp");
                                lspRow.name = $(this).find("name").contents().get(0).nodeValue;

                                if ($(this).find("lsp-state").text() === "Up")
                                    lspRow.lspState = true;
                                else
                                    lspRow.lspState = false;

                                lspRow.activePath = $(this).find("active-path").text();
                                lspRow.egressLabelOperation = $(this).find("egress-label-operation").text();
                                lspRow.loadBalance = $(this).find("load-balance").text();

                                var source_addr = $(this).find("source-address").text();

                                $(this).find("mpls-lsp-path").each(function() {
                                    //console.log("mpls-lsp-path");
                                    var received_rro = $(this).find("received-rro").text();
                                    var address_arr = [];
                                    //console.log("received_rro is not null or blank");
                                    received_rro = received_rro.substr(received_rro.indexOf(":") + 1);
                                    received_rro = received_rro.trim();
                                    address_arr = received_rro.split(" ");
                                    address_arr.unshift(source_addr.replace("128", "10")); //require change
                                    //console.log(address_arr);

                                    lspRow.path = address_arr;

                                    for (i = 0; i < address_arr.length; i++) {
                                        if (!(address_arr[i] in lookUpMap)) {
                                            lookUpMap[address_arr[i]] = [];
                                        }
                                    }

                                    address_arr = [];
                                    lspRow.myhost = source_addr.replace("128", "10");
                                    lspRow.bandwidth = $(this).find("bandwidth").text();
                                    lspRow.hoplimit = $(this).find("hoplimit").text();
                                    lspRow.smartOptimizeTimer = $(this).find("smart-optimize-timer").text();
                                    lspRow.lsp_Type = $(this).find("lsp-type").text();
                                    lspRow.locationInfo = [];
                                    received_rro = "";
                                });
                                lspDict.ingress.push(lspRow);
                                lspRow = {};
                            });
                        } 

                        //check if session_type is 'Egress' and populate egress JSON
                        else if (session_type === "Egress") {
                            lspRow.name = $(this).find("name").text();
                            var record_route = $(this).find("record-route").text();

                            if ($(this).find("lsp-state").text() === "Up")
                                lspRow.lspState = true;
                            else
                                lspRow.lspState = false;

                            //console.log("Egress record_route:" + record_route);

                            var addresss_arr = [];
                            address_arr = $(this).find("address").map(function() {
                                return $(this).text();
                            }).get();

                            address_arr.push($(this).find("destination-address").text());
                            //console.log(address_arr);
                            lspRow.path = address_arr;

                            for (i = 0; i < address_arr.length; i++) {
                                if (!(address_arr[i] in lookUpMap)) {
                                    lookUpMap[address_arr[i]] = [];
                                }
                            }
                            address_arr = [];
                            lspRow.myhost = $(this).find("destination-address").text();
                            lspRow.lspPathType = $(this).find("lsp-path-type").text();
                            lspRow.rsbCount = $(this).find("rsb-count").text();
                            lspRow.senderTspec = $(this).find("sender-tspec").text();
                            lspRow.psbCreationTime = $(this).find("psb-creation-time").text();
                            lspRow.lspId = $(this).find("lsp-id").text();
                            lspRow.tunnelId = $(this).find("tunnel-id").text();
                            lspRow.adspec = $(this).find("adspec").text();
                            lspRow.psbLifetime = $(this).find("psb-lifetime").text();
                            lspRow.resvStyle = $(this).find("resv-style").text();
                            lspRow.labelIn = $(this).find("label-in").text();
                            lspRow.labelOut = $(this).find("label-out").text();
                            lspDict.egress.push(lspRow);
                            lspRow.locationInfo = [];
                            lspRow = {};
                        }
                        //check if session_type is 'Transit' and populate transit JSON
                        else if ((session_type === "Transit")) {
                            lspRow.name = $(this).find("name").text();
                            if ($(this).find("lsp-state").text() === "Up")
                                lspRow.lspState = true;
                            else
                                lspRow.lspState = false;
                            var record_route = $(this).find("record-route").text();
                            //console.log("Transit record_route:" + record_route);
                            var addresss_arr = [];
                            address_arr = record_route.split("\n");
                            address_arr.shift();  // Removes the first element from an array and returns only that element.
                            address_arr.pop();
                            
                            //console.log(address_arr);
                            lspRow.path = address_arr;

                            for (i = 0; i < address_arr.length; i++) {
                                if (!(address_arr[i] in lookUpMap) && address_arr[i]!="") {
                                    lookUpMap[address_arr[i]] = [];
                                }
                            }

                            address_arr = [];
                            lspRow.lspPathType = $(this).find("lsp-path-type").text();
                            lspRow.rsbCount = $(this).find("rsb-count").text();
                            lspRow.senderTspec = $(this).find("sender-tspec").text();
                            lspRow.psbCreationTime = $(this).find("psb-creation-time").text();
                            lspRow.lspId = $(this).find("lsp-id").text();
                            lspRow.tunnelId = $(this).find("tunnel-id").text();
                            lspRow.adspec = $(this).find("adspec").text();
                            lspRow.psbLifetime = $(this).find("psb-lifetime").text();
                            lspRow.resvStyle = $(this).find("resv-style").text();
                            lspRow.labelIn = $(this).find("label-in").text();
                            lspRow.labelOut = $(this).find("label-out").text();
                            lspDict.transit.push(lspRow);
                            lspRow.locationInfo = [];
                            lspRow = {};
                        }
                    });
                });

                if (resultStatus) {
                    //console.log("resultStatus is true");
                    //if result status is true call getLSPInfoJSON
                    getLSPInfoJSON(lspDict, device, view);
                } else {
                    //if result status is false call again getchcLSPInfo
                    fetchLSPInfo(view, device);
                    //console.log("resultStatus is false");
                }
            } else {
                view.set('controller.error', "RPC failed.");
            }
        });
    }

    //makeID() function generates a random string of 5 chars
    function makeID() {
        var text = "";
        var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

        for (var i = 0; i < 5; i++)
            text += possible.charAt(Math.floor(Math.random() * possible.length));

        return text;
    }

    //The getInterfaceStats function fetches the interface statistics by
    //running the command show interface statistics detail
    function getInterfaceStats(view, device, interfaceName) {
        //console.log("In getInterfaceStats:" + device + " interfaceName:" + interfaceName);
        var cmd = "show interfaces statistics detail " + interfaceName;
        var res = null;
        var inRate = 0;
        var outRate = 0;
        var inBytes = 0;
        var outBytes = 0;
        var speed = 0;
        view = currentView;

        $.clira.runCommand(view, device.toLowerCase().substring(0, 2), cmd, "json", function(view, status,
            result) {
            if (status) {
                //console.log("In getInterfaceStats callback");
                if (result) {
                    //console.log("In getInterfaceStats result");
                    res = $.parseJSON(result);
                    ////console.log(JSON.stringify(res));
                    view.set('controller.dialogLoading', false);
                    traffic_statistics = res["interface-information"][0]["logical-interface"][0]["traffic-statistics"][0];
                    inBytes = traffic_statistics["input-bytes"][0].data;
                    outBytes = traffic_statistics["output-bytes"][0].data;
                    //speed = res["interface-information"][0][0]["speed"][0].data;

                    $("#routerInfo").html(device + " " + interfaceName);
                    $("#inputPackets").html("<b style='font-size:20px;color:black;' id='inputPacketsTitle'>Input Packets</b><br/>" + res["interface-information"][0]["logical-interface"][0]["traffic-statistics"][0]["input-packets"][0].data);
                    $("#outputPackets").html("<b style='font-size:20px;color:black;' id='outputPacketsTitle'>Output Packets</b><br/>" + res["interface-information"][0]["logical-interface"][0]["traffic-statistics"][0]["output-packets"][0].data);
                    $("#inputBytes").html("<b style='font-size:20px;color:#c7bbc9;id='inputBytesTitle'>Input Bytes</b><br/>" + res["interface-information"][0]["logical-interface"][0]["traffic-statistics"][0]["input-bytes"][0].data);
                    $("#outputBytes").html("<b style='font-size:20px;color:#c7bbc9;' id='outputBytesTitle'>Output Bytes</b><br/>" + res["interface-information"][0]["logical-interface"][0]["traffic-statistics"][0]["output-bytes"][0].data);

                    if (inRate.current < 0) inRate.current = 0;
                    if (outRate.current < 0) outRate.current = 0;
                }
            }
        });
    }

    //populateLSPJsonWithLocation populates the lspDict by running the slax
    //script and fetching the location information 
    function populateLSPJsonWithLocation(lspTypeList, currentLSPType, iter, lspDict, device,transitFlag, view,callback) {

        //console.log('In populateLSPJsonWithLocation');
        //console.log("Device:"+device);

        var lspType = lspTypeList[currentLSPType];
        if( lspType === "transit")
        {
            transitFlag = "true";
        }

        if (lspDict[lspType].length) {
            var hostList = lspDict[lspType][iter].path.join(",");
            //console.log("hostlist:" + hostList);
            $.clira.runSlax({
                create: "yes",
                script: '/apps/map-mpls-lsp/map-mpls-lsp.slax',
                args: {
                    oper: "getlocation",
                    iplist: hostList,
                    port: 22,
                    save_password: 1,
                    username: "regress",
                    prefix: lspType + makeID(),
                    device: device,
                    transitFlag:transitFlag
                },
                view: view,
                success: function(data) {
                    //console.log("In success lspTypeList:" + lspTypeList);
                    //console.log("currentLSPType:" + currentLSPType + " iter:" + iter);
                    var result = $.parseJSON(data);
                    if (result != null) {
                        lspDict[lspType][iter].locationInfo = result.location_info;
                        
                        for (i = 0; i < result.location_info.length; i++) {
                            if (result.location_info[i].ip in lookUpMap)
                                lookUpMap[result.location_info[i].ip][0] = result.location_info[i].cliradevicename;
                            
                            if((transitFlag == "true") && (device == result.location_info[i].cliradevicename))
                            {
                                //console.log("In if populateLSPJsonWithLocation transitFlag");
                                lookUpMap[result.location_info[i].ip] = [];
                                lookUpMap[result.location_info[i].ip][0] = result.location_info[i].cliradevicename;
                                lspDict[lspType][iter].path[lspDict[lspType][iter].path.indexOf("")] = result.location_info[i].ip;
                                lspDict[lspType][iter].myhost = result.location_info[i].ip;
                            }
                        }
                    }

                    if (currentLSPType == (lspTypeList.length) - 1 && iter == (lspDict[lspType].length - 1)) {
                        //console.log("Calling callback from populateLSPJsonWithLocation");
                        //console.log("populateLSPJsonWithLocation CurrentLSPType:" + currentLSPType + " iter:" + iter);
                        callback(lspDict);
                    }

                    if (iter == (lspDict[lspType].length - 1)) {
                        if (currentLSPType != (lspTypeList.length - 1)) {
                            currentLSPType++;
                            iter = 0;
                            populateLSPJsonWithLocation(lspTypeList, currentLSPType, iter, lspDict, device,transitFlag, view, callback);
                        }
                    } else {
                        iter++;
                        populateLSPJsonWithLocation(lspTypeList, currentLSPType, iter, lspDict, device,transitFlag, view, callback);
                    }

                },
                failure: function(data) {
                    console.error("Error in executing SLAX script");
                    $.clira.makeAlert(view, "Error executing command: " + data);
                }
            });
        }

    }


    //The generateMap function is an action function which is bound to the
    //on click event of the show lsp on map button
    var generateMap = function(context) {
        //console.log("In GenerateMap");
        var map;
        var gmaps;
        var mapDiv = $("#mapView");
        var tempMarkerArr = [];
        var tempPathArr = [];
        map = null;
        gmaps = {};
        mapDiv = $("#mapView");
        mapDiv.empty();
        var polyLineColor = "#ff0000";
        var lineSymbolColor = "#393";
        if (context) {
            //console.log("In Context");
            mapDiv.css('position', "relative");
            mapDiv.css('height', context.height);
            var bounds = new google.maps.LatLngBounds();
            for (var i = 0; i < context.path.length; i++) {
                bounds.extend(new google.maps.LatLng(context.path[i][0], context.path[i][1]));
            }

            gmaps = {
                div: "#mapView",
                lat: "37.4083391",
                lng: "-122.0289092",
                mapTypeId: google.maps.MapTypeId.TERRAIN,
                idle: function() {
                    map.refresh();
                },
                resize: function() {
                    var center = map.getCenter();
                    map.setCenter(center.lat(), center.lng());
                }
            };

            map = new GMaps(gmaps);

            if (context.lspType == "INGRESS") {
                polyLineColor = "#c76618";
                lineSymbolColor = "#393";
            } else if (context.lspType == "EGRESS") {
                polyLineColor = "#002bff";
                lineSymbolColor = "#ff0000";
            } else {
                polyLineColor = "#00ff45";
                lineSymbolColor = "#4900ff";
            }

            var lineSymbol = {
                path: google.maps.SymbolPath.CIRCLE,
                scale: 8,
                strokeColor: lineSymbolColor
            };

            tempMarkerArr = context.markersArr;
            tempIndex = 0;
            
            for(i=0; i<context.path.length; i++)
            {
               
                tempPathArr[i] = context.path[context.order.indexOf(i)];
            }

            for (i = 0; i < context.markersArr.length; i++) {
                context.markersArr[i].lat = context.path[i][0];
                context.markersArr[i].lng = context.path[i][1];
            }

            map.fitBounds(bounds);
            map.addMarkers(context.markersArr);

            var polyline = map.drawPolyline({
                path: tempPathArr,
                strokeColor: polyLineColor,
                strokeOpacity: 0.6,
                strokeWeight: 6,
                icons: [{
                    icon: lineSymbol,
                    offset: '100%'
                }]
            });

            animateCircle();
        }

        function animateCircle() {
            var count = 0;
            window.setInterval(function() {
                count = (count + 1) % 200;
                var icons = polyline.get('icons');
                icons[0].offset = (count / 2) + '%';
                polyline.set('icons', icons);
            }, 20);
        }

    };
    
    //The getLSPInfoJSON fetches the all the LSP information in recursive
    //callbacks
    function getLSPInfoJSON(lspDict,device, view) {
        var lspTypesList = [];

        if (lspDict.ingress.length) {
            lspTypesList.push("ingress");
        }

        if (lspDict.egress.length) {
            lspTypesList.push("egress");
        }

        if (lspDict.transit.length) {
            lspTypesList.push("transit");
        }

        if(lspDict.ingress.length || lspDict.egress.length || lspDict.transit.length)
        {
            lspDict.lspExists = true;
        }

        else
        {
            lspDict.lspExists = false;
        }

        if (lspTypesList.length) {
            populateLSPJsonWithLocation(lspTypesList, 0, 0, lspDict, device,"false", view, function(lspDict) {
                //console.log("In populateLSPJsonWithLocation callback");
                //console.log(lspDict);

                getHostnames(lookUpMap, lspDict, view, function(lspDict, view) {
                    //console.log("In getHostnames callback");

                    getInterfaceNames(lookUpMap,lspDict, view, function(lspDict, view)
                    {
                        //console.log("in getInterfaceNames callback");
                        fetchAddressFromCoordinates(lspDict, view, function(lspDict, view) {
                            //console.log("fetchAddressFromCoordinates result");
                            view.set('controller.loading', false);
                            view.set('controller.lspDict', lspDict);
                            view.get('controller')._actions.generateMap = generateMap;
                        });
                    });

                });

            });
        } else {
            view.set('controller.error', "No LSP Found.");
        }
    }

    //The getHostNames fetches the hostname of the device and
    //populates the lookUpMap
    function getHostnames(lookUpMap, lspDict, view, callback) {
        //console.log("In getHostnames");
        var cmd = "show configuration groups re0 system host-name";
        var count = [];
        for (key in lookUpMap) {
            (function(key, lookUpMap, count) {
                $.clira.runCommand(view, lookUpMap[key][0], cmd, "xml", function(view, status,
                    result) {
                    if (status) {
                        //console.log("In runCommand Callback:" + lookUpMap[key][0]);
                        //console.log(result);
                        hostname = $(result).find("configuration-output").text();
                        hostname = hostname.substring(11, hostname.length - 2);
                        //console.log(hostname);
                        count.push(hostname);
                        //console.log("Count array length:"+count.length);
                        lookUpMap[key][1] = hostname;
                        //console.log("lookUpMap length:" + Object.keys(lookUpMap).length);
                        //console.log(lookUpMap);
                        if (count.length == Object.keys(lookUpMap).length) {
                            callback(lspDict, view, lookUpMap);
                        }
                    }
                });
            })(key, lookUpMap, count);
        }
    }


    //The getInterfaceNames fetches the interface name of the device and
    //populates the lookUpMap 
    function getInterfaceNames(lookUpMap, lspDict, view, callback) {
        //console.log("In getInterfaceNames");
        var count = [];
        var cmd = "show interfaces terse";
        for (key in lookUpMap) {
                (function(key, lookUpMap, count) {
                    $.clira.runCommand(view, lookUpMap[key][0], cmd, "json", function(view, status,
                    result) 
                    {
                        if (status) {
                        //console.log("In getInterfaceNames callback");
                            var res = null;
                            if (result) {
                                //console.log("In getInterfaceNames result");
                                try {
                                    res = $.parseJSON(result);
                                    count.push(count.length+1); 
                                    var physical_interface = res["interface-information"][0]["physical-interface"];
                                    var logical_interface = null;
                                    var address_family = null;
                                    for(i=0; i < physical_interface.length; i++)
                                    {
                                        if(physical_interface[i].hasOwnProperty("logical-interface"))
                                        {
                                            //console.log("logical_interface property exists");
                                            logical_interface = physical_interface[i]["logical-interface"][0];
                                            if(logical_interface.hasOwnProperty("address-family"))
                                            {

                                                if(logical_interface["address-family"][0].hasOwnProperty("interface-address"))
                                                {
                                                    interface_address = field(logical_interface["address-family"][0]["interface-address"][0],"ifa-local").replace("128","10");
                                                    if(interface_address.indexOf(key) > -1)
                                                    {
                                                        //console.log("Interface Name:"+field(logical_interface,"name")+" interface_address:"+key);
                                                        lookUpMap[key][2] = field(logical_interface,"name");
                                                        break;
                                                    }
                        
                                                }

                                                interface_address = "";
                                            }
                                        }
                                        logical_interface = null;
                                        address_family = null;
                                    }  
                                }
                                catch(err) {
                                    //console.log("Error in JSON data recevied in getInterfaceNames");
                                    lookUpMap[key][2] = "fxp0.0";
                                    count.push(count.length+1); 
                                }
                                
                            }
                            
                            if (count.length == Object.keys(lookUpMap).length) 
                            {
                                callback(lspDict, view, lookUpMap);
                            }
                        }
                    });
                })(key, lookUpMap, count);
        }

    }
  
    //field object is a helper function
    function field(obj, prop) {
        if (obj.hasOwnProperty(prop) && $.isArray(obj[prop]) && obj[prop][0].hasOwnProperty('data')) {
            return obj[prop][0].data;
        }
        return "N/A";
    }

    //The fetchAddressFromCoordinates function calls the google maps api
    //to fetch the address of the device by passing the lat and long values
    function fetchAddressFromCoordinates(lspDict, view, callback) {
        //console.log("In fetchAddressFromCoordinates");
        var latlng = "";
        var base_url = "http://maps.googleapis.com/maps/api/geocode/json?latlng=";
        var lspInfoArray = [];
        var lspInfoRow = {};
        var path = [];
        var markersArr = [];
        var pathArr = [];
        var count = [];
        var numIter = 0;
        var icon = "";
        var orderArr = [];
        for (var key in lspDict) {
            if (lspDict.hasOwnProperty(key)) {
                for (i = 0; i < lspDict[key].length; i++) {
                    numIter += lspDict[key][i].locationInfo.length;
                }
            }
        }

        for (var key in lspDict) {

            if (lspDict.hasOwnProperty(key) && key != "lspExists") {
                for (i = 0; i < lspDict[key].length; i++) {
                    (function(key, lspDict) {
                        (function(i, lspDict, key) {
                            markersArr = [];
                            pathArr = [];
                            orderArr = [];
                            for (j = 0; j < lspDict[key][i].locationInfo.length; j++) {
                                if(!(lspDict[key][i].locationInfo[j].latitude == "" || lspDict[key][i].locationInfo[j].longitude == ""))
                                {
                                    //console.log("URL: i<" + i + "> j<" + j + ">" + base_url + lspDict[key][i].locationInfo[j].latitude + ',' + lspDict[key][i].locationInfo[j].longitude + "&sensor=false");
                                    (function(j, markersArr, pathArr, orderArr, count) {
                                        $.getJSON(base_url + lspDict[key][i].locationInfo[j].latitude + ',' + lspDict[key][i].locationInfo[j].longitude + "&sensor=false",
                                            function(json) {
                                                if (json.status == "OK") {
                                                    status = true;
                                                    var res = json.results[0];
                                                    //console.log("before Count:<" + count.length + ">");
                                                    count.push(j);
                                                    //console.log("After Count:<" + count.length + ">");

                                                    if (lspDict[key][i].myhost == lspDict[key][i].locationInfo[j].ip)
                                                        icon = "/apps/map-mpls-lsp/images/myrouter.png";
                                                    else
                                                        icon = "/apps/map-mpls-lsp/images/router.png";

                                                    markersArr.push({
                                                        lat: lspDict[key][i].locationInfo[j].latitude,
                                                        lng: lspDict[key][i].locationInfo[j].longitude,
                                                        title: "Device:" + lookUpMap[lspDict[key][i].locationInfo[j].ip][1],
                                                        icon: icon,
                                                        animation: google.maps.Animation.DROP,
                                                        infoWindow: {
                                                            content: "<p><b>Device IP:</b>" + lspDict[key][i].locationInfo[j].ip + "<br/><b> Device Name:" + lookUpMap[lspDict[key][i].locationInfo[j].ip][1] + "</b><br/><b>Device Address:</b>" + res.formatted_address + "</p>"
                                                        }
                                                    })

                                                    pathArr.push(

                                                        [lspDict[key][i].locationInfo[j].latitude, lspDict[key][i].locationInfo[j].longitude]
                                                    );

                                                    orderArr.push(j);

                                                    //console.log(JSON.stringify(markersArr));
                                                    lspInfoRow.address = markersArr[0].title;
                                                    lspInfoRow.lat = markersArr[0].lat;
                                                    lspInfoRow.lng = markersArr[0].lng;
                                                    lspInfoRow.height = "450px";
                                                    lspInfoRow.markersArr = markersArr;
                                                    lspInfoRow.path = pathArr;
                                                    lspInfoRow.lspName = lspDict[key][i].name;
                                                    lspInfoRow.lspType = key.toUpperCase();
                                                    lspInfoRow.hide = true;
                                                    lspInfoRow.order = orderArr;

                                                    //view.get('controller').set('output', output);
                                                    lspDict[key][i].output = (lspInfoRow);
                                                    lspInfoRow = {};
                                                    markersArr = [];
                                                    path = {};
                                                    pathArr = [];
                                                    orderArr = [];
                                                    //console.log("count.length" + count.length);

                                                    if (count.length == numIter) {
                                                        //console.log("Calling the callback  | key<" + key + "> i:<" + i + "> j:<" + j + ">");

                                                        callback(lspDict, view);
                                                    }

                                                } else if (json.status == "ZERO_RESULTS") {
                                                    lspDict.error = {
                                                        message: "Location not found: " + me.data.location,
                                                        type: "error"
                                                    };
                                                } else {
                                                    if (json.status) {
                                                        lspDict.error = {
                                                            message: "geo failure",
                                                            type: "error"
                                                        };
                                                    } else {
                                                        lspDict.error = {
                                                            message: json.status,
                                                            type: "error"
                                                        };
                                                    }
                                                }

                                            });
                                    })(j, markersArr, pathArr,orderArr, count);
                                }
                            }
                        })(i, lspDict, key);
                    })(key, lspDict);

                }

            }

        }
        //console.log("In end");
        //console.log(JSON.stringify(lspDict));
        callback(lspDict, view);
    }
});
