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

        cliraInit: function cliraInit (need_history, submit) {
            $.clira.prefsInit();
            $.clira.decorateIcons($("#debug-container"));

            if (need_history) {
                $.clira.cmdHistory = $.mruPulldown({
                    name: "command-history",
                    top: $("#command-top"),
                    clearIcon: $("#command-clear"),
                    entryClass: "command-history-entry",
                    click: function (me) {
                        if (tgtHistory)
                            tgtHistory.close();
                    }
                });

                tgtHistory = $.mruPulldown({
                    name: "target-history",
                    top: $("#target-top"),
                    clearIcon: $("#target-clear"),
                    multiple: $("#target-history-form"),
                    history: $("#target-history"),
                    entryClass: "target-history-entry",
                    focusAfter: $("#command-input"),
                    click: function (me) {
                        if ($.clira.cmdHistory)
                            $.clira.cmdHistory.close();
                    }
                });
            }

            $("#target-input-form").submit(submit);
            $("#command-input-form").submit(submit);

            $("#input-enter").text("Enter").button({
                text: true
            }).click(function (e) {
                submit(e);
                $.clira.cmdHistory.focus();
                $(this).blur();
                return false;
            });

            $("#juise").button();

            if (localStorage['debug'] 
                    && JSON.parse(localStorage['debug']) == false) {
                $("#debug-container").css({ display: "none" });
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

    function cliInit () {
        cmdHistory = $.mruPulldown({
            name: "command-history",
            top: $("#command-top"),
            clearIcon: $("#command-clear"),
            entryClass: "command-history-entry",
            click: function (me) {
                if (tgtHistory)
                    tgtHistory.close();
            }
        });

        tgtHistory = $.mruPulldown({
            name: "target-history",
            top: $("#target-top"),
            clearIcon: $("#target-clear"),
            multiple: $("#target-history-form"),
            history: $("#target-history"),
            entryClass: "target-history-entry",
            focusAfter: $("#command-input"),
            click: function (me) {
                if (cmdHistory)
                    cmdHistory.close();
            }
        });

        $("#target-input-form").submit(commandSubmit);
        $("#command-input-form").submit(commandSubmit);

        $("#input-enter").text("Enter").button({
            text: true
        }).click(function (e) {
            commandSubmit(e);
            cmdHistory.focus();
            $(this).blur();
            return false;
        });

        $.clira.prefsInit();
    }

    function commandSubmit (event) {
        event.preventDefault();
        $.dbgpr("commandSubmit:", event.type);

        if (tgtHistory)
            tgtHistory.close();
        $.clira.cmdHistory.close();
  
        var command = $.clira.cmdHistory.value();
        if (command == "") {
            $.dbgpr("submit; skipping since command value is empty");
            $.clira.cmdHistory.focus();
            return false;
        }

        //
        // Since multiple targets are allowed in the target field,
        // we need to break the target string up into distinct values.
        //
        var count = 0;
        var tset = [ ];
        var tname = "";
        var cname = "";
        var value = tgtHistory.value();

        $.dbgpr("commandSubmit: target [", target,
                "] command [", command, "]");

        if (value == "") {
            $.dbgpr("submit: no target, but still working");
            commmandWrapperAdd(undefined, command);

        } else {
            $(value.split(" ")).each(function (i, x) {
                if (x != "") {
                    count += 1;
                    tset.push(x);
                    commmandWrapperAdd(x, command);
                    tgtHistory.markUsed(x);
                    tname += " " + x;
                    cname += "_" + x.replace("_", "__", "g");
                }
            });

            tname = tname.substr(1);
            cname = cname.substr(1);
            if (cname && cname != "")
                $.clira.targetListMarkUsed(tname, cname, null);
        }

        $.clira.cmdHistory.markUsed(command);
        $.clira.commandOutputTrim(count);

        return false;
    }

    function parseParams (cmdline) {
        var argv = cmdline.split(" ");
        var params = { };
        for (var i = 0; i < argv.length; i += 2) {
            params[argv[i]] = argv[i + 1];
        }
        return params;
    }

    function commmandWrapperAdd (target, command) {
        var test;
        var local;

        $.dbgpr("commmandWrapperAdd", target, command);

        var content = "<div class='output-wrapper ui-widget "
            +     "ui-widget-content ui-corner-all'>"
            + "<div class='output-header ui-state-default "
            +     "ui-widget-header ui-corner-all'>"
            + "<button class='icon-remove-section'></button>"
            + "<button class='icon-hide-section hidden'></button>"
            + "<button class='icon-unhide-section'></button>"
            + "<button class='keeper icon-keeper-section'></button>";

        if (target) {
            content += "<div class='target-value rounded buttonish green'>"
                + target
                + "</div><div class='label'> -> </div>";
        }

        content += "<div class='command-value rounded blue'>"
            + command
            + "</div><div class='command-number'>"
            + " (" + ++command_number + ")"
            + "</div></div><div class='output-content can-hide'>"

        if (command.substring(0, 10) == "test form ") {
            test = command.substring(10);
            if (testForms[test]) {
                content += "<div class='ui-yform'></div>";
            } else {
                test = undefined;
            }
            content += "<div class='output-replace'></div>";

        } else if (command.substring(0, 6) == "local ") {
            local = command.substring(6);
            content += "<div class='output-replace'>"
                + loadingMessage;
                + "</div>";

        } else if ($.clira.prefs.live_action) {
            content += "<div class='output-replace'>"
                + loadingMessage;
                + "</div>";

        } else {
            content += "<div class='output-replace'>"
                + "test-output\nmore\nand more and more\noutput\n";
                + "</div>";
        }

        content += "</div>";

        var $newp = jQuery(content);
        $output.prepend($newp);
        if ($.clira.prefs.slide_speed > 0)
            $newp.slideUp(0).slideDown($.clira.prefs.slide_speed);
        divUnhide($newp);

        if (test) {
            var yd = $("div.ui-yform", $newp);
            var yf = $.yform(testForms[test], yd);

            yf.buildForm(function (yform, guide) {
                hideCommandForm(yform);
                var $out = yform.form.parents("div.output-content")
                .find("div.output-replace");
                $out.html(loadingMessage);

                var rpc = yform.buildRpc();
                runRpc(yform, $out, rpc, tgtHistory.value());
            });
            yf.focus();

        } else if (local) {
            var $out = $("div.output-replace", $newp);
            $out.slideUp(0).slideDown($.clira.prefs.slide_speed);

            var params = parseParams("script " + local);
            var name = params["script"];

            $out.load("/local/" + name, params,
                      function (text, status, http) {
                          loadHttpReply(text, status, http,
                                        $(this), $out);
                      });

        } else if ($.clira.prefs.live_action) {
            var $out = $("div.output-replace", $newp);
            if ($.clira.prefs.muxer) {
                $.clira.runCommand($out, target, command);

                if (muxer == undefined) {
                    openMuxer();
                }

                // Resolve our group/device
                $.getJSON(
                    "/clira/db.php?p=group_members",
                    { target: target },
                    function (json) {
                        $.each(json.devices, function (i, name) {
                            $.clira.runCommand($out, name, command);
                        });
                    }
                ).fail(function fail (x, message, err) {
                    // Failure means we assume it's a single target
                    $.clira.runCommand($out, target, command);
                });
            }
        }

        $.clira.decorateIcons($newp);

        if (target) {
            $(".target-value", $newp).click(function () {
                tgtHistory.close();
                tgtHistory.select($(this).text());
                $.clira.cmdHistory.focus();
            });
        }

        $(".command-value", $newp).click(function () {
            $.clira.cmdHistory.close();
            $.clira.cmdHistory.select($(this).text());
            $.clira.cmdHistory.focus();
        });

        return false;
    }

    function openMuxer () {
        if (muxer)
            muxer.close();

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
                muxer.failed = true;
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
                view.append();
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

            var full = [ ];

            muxer.rpc({
                div: output,
                target: target,
                payload: "<command format='html'>" + command + "</command>",
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
                    if (full.length <= 2) {
                        if (domElement) {
                            output.html(data);
                        } else {
                            view.get('controller').set('output', data);
                        }
                    }
                },
                oncomplete: function () {
                    $.dbgpr("rpc: complete");
                    if (domElement) {
                        output.html(full.join(""));
                    } else {
                        view.get('controller').set('completed', true);
                        view.get('controller').set('output', full.join(""));
                    }

                    if ($.isFunction(onComplete)) {
                        onComplete(true, output);
                    }
                },
                onhostkey: function (data) {
                    var self = this;
                    promptForHostKey(view, target, data, function (response) {
                        muxer.hostkey(self, response);
                    });
                },
                onpsphrase: function (data) {
                    var self = this;
                    promptForSecret(view, target, data, function (response) {
                        muxer.psphrase(self, response);
                    }, function() {
                        muxer.close();
                    });
                },
                onpsword: function (data) {
                    var self = this;
                    promptForSecret(view, target, data, function (response) {
                        muxer.psword(self, response);
                    }, function() {
                        muxer.close();
                    });
                },
                onclose: function (event, message) {
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

    function promptForHostKey (view, target, prompt, onclick) {
        var hostKeyView = Clira.DynFormView.extend({
            title: "Host key for " + target,
            buttons: {
                Accept: function() {
                    onclick("yes");
                    $(this).dialog("close");
                },
                Decline: function() {
                    onclick("no");
                    $(this).dialog("close");
                }
            },
            close: function() {
                onclick("no");
            }
        }); 
        view.createChildView(hostKeyView, {message: prompt}).append();
    }

    function promptForSecret (view, target, prompt, onclick, onclose) {
        var title = "Password: ";
        $.ajax({
            url: 'db.php?p=device&name=' + target,
            success: function(result) {
                title += result['username'] + '@' + result['hostname'];
            },
            async: false
        });
        var secretView = Clira.DynFormView.extend({
            title: title,
            buttons: {
                Enter: function() {
                    onclick(viewContext.get('fieldValues').password);
                    $(this).context.enter = true;
                    $(this).dialog("close");
                },
                Cancel: function() {
                    $(this).dialog("close");
                }
            },
            close: function() {
                if (!this.$().context.enter)
                    onclose();
            }
        });
        var fields = [{
            name: "password",
            title: "",
            secret: true
        }];
        view.createChildView(secretView, {fields: fields, 
                            message: prompt.split(/(?:\n)+/)}).append();
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
        if ($.clira.prefs.live_action) {
            $output.slideUp(0).slideDown($.clira.prefs.slide_speed);
            $output.load("/clira/clira.slax",
                         {
                             target: target, // target is optional
                             rpc: rpc,       // rpc is in string form
                             form: "false"   // don't want form in reply
                         },
                         function (text, status, http) {
                             loadHttpReply(text, status, http,
                                           $(this), $output);
                         });
        } else {
            $output.text("RPC:\n" + rpc);
        }
    }

    if (false) {
        $.clira.cliraInit(true, commandSubmit);
        tgtHistory.focus();
    }
});
