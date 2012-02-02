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
    decorateIcons($("#debug-container"));
    $.dbgpr("document is ready");

    var $output = $("#output-top");
    var command_number = 0;
    var cmdHistory, tgtHistory;
    var prefs = { };            // Initial value (to handle early references)

    var loadingMessage = "<img src='/icons/loading.png'>"
        + "    ....loading...\n";

    var prefs_fields = [
        {
            name: "output_close_after",
            def: 5,
            type: "number",
            change: commandOutputTrimChanged,
            title: "Number of open commands",
        },
        {
            name: "output_remove_after",
            def: 10,
            type: "number",
            change: commandOutputTrimChanged,
            title: "Total number of commands",
        },
        {
            name: "slide_speed",
            def: 0, // "fast",
            type: "number-plus",
            title: "Speed of slide animations",
        },
        {
            name: "max_commands_list",
            def: 40,
            type: "number",
            title: "Number of commands kept in list",
        },
        {
            name: "max_targets_inline",
            def: 10,
            type: "number",
            title: "Number of targets kept on screen",
        },
        {
            name: "max_targets_list",
            def: 40,
            type: "number",
            title: "Number of targets kept in list",
        },
        {
            name: "max_target_position",
            def: 10,
            type: "number",
            title: "Target button rows",
        },
        {
            name: "stay_on_page",
            def: false,
            type: "boolean",
            label: "Stay",
            change: prefsSetupConfirmExit,
            title: "Give warning if leaving this page",
        },
        {
            name: "theme",
            def: "black-tie",
            type: "string",
            title: "UI Theme",
            change: prefsSwitchTheme,
        },
        {
            name: "live_action",
            def: false,
            type: "boolean",
            label: "Live",
            title: "Interact with real devices",
        },
    ];

    var prefs_options = {
        preferences: {
            apply: function () {
                $.dbgpr("prefsApply");
                prefs_form.close();
                prefs = prefs_form.getData();
            }
        },
        title: "Preferences",
        dialog : { },
    }

    var testForms = {
        first: {
            title: "Ping Options",
            command: {
                rpc: "ping",
            },
            tabs: { },
            css: "/clira/ping.css",
            fieldsets: [
                {
                    legend: "Basic",
                    fields: [
                        {
                            name: "host",
                            title: "Hostname or IP address of remote host",
                            type: "text",
                        },
                        {
                            name: "count",
                            title: "Number of ping requests to send",
                            type: "number",
                            range: "1..2000000000",
                            units: "packets",
                        },
                        {
                            name: "interval",
                            title: "Delay between ping requests",
                            units: "seconds",
                            type: "number",
                        },
                        {
                            name: "no-resolve",
                            title: "Don't attempt to print addresses symbolically",
                            type: "boolean",
                        },
                        {
                            name: "size",
                            title: "Size of request packets",
                            type: "number",
                            units: "bytes",
                            range: "0..65468",
                        },
                        {
                            name: "wait",
                            title: "Delay after sending last packet",
                            type: "number",
                            units: "seconds",
                        },
                    ],
                },
                {
                    legend: "Outgoing",
                    fields: [
                        {
                            name: "bypass-routing",
                            title: "Bypass routing table, use specified interface",
                            type: "boolean",
                        },
                        {
                            name: "do-not-fragment",
                            title: "Don't fragment echo request packets (IPv4)",
                            type: "boolean",
                        },
                        {
                            name: "inet",
                            title: "Force ping to IPv4 destination",
                            type: "boolean",
                        },
                        {
                            name: "inet6",
                            title: "Force ping to IPv6 destination",
                            type: "boolean",
                        },
                        {
                            name: "logical-system",
                            title: "Name of logical system",
                            type: "string",
                        },
                        {
                            name: "interface",
                            title: "Source interface (multicast, all-ones, unrouted packets)",
                            type: "interface-name",
                        },
                        {
                            name: "routing-instance",
                            title: "Routing instance for ping attempt",
                            type: "string",
                        },
                        {
                            name: "source",
                            title: "Source address of echo request",
                            type: "ip-address",
                        },
                    ],
                },
                {
                    legend: "ICMP",
                    fields: [
                        {
                            name: "loose-source",
                            title: "Intermediate loose source route entry (IPv4)",
                            type: "boolean",
                        },
                        {
                            name: "record-route",
                            title: "Record and report packet's path (IPv4)",
                            type: "boolean",
                        },
                        {
                            name: "strict",
                            title: "Use strict source route option (IPv4)",
                            type: "boolean",
                        },
                        {
                            name: "strict-source",
                            title: "Intermediate strict source route entry (IPv4)",
                            type: "boolean",
                        },
                    ],
                },
                {
                    legend: "Advanced",
                    fields: [
                        {
                            name: "detail",
                            title: "Display incoming interface of received packet",
                            type: "boolean",
                        },
                        {
                            name: "verbose",
                            title: "Display detailed output",
                            type: "boolean",
                        },
                        {
                            name: "mac-address",
                            title: "MAC address of the nexthop in xx:xx:xx:xx:xx:xx format",
                            type: "mac-address",
                        },
                        {
                            name: "pattern",
                            title: "Hexadecimal fill pattern",
                            type: "string",
                        },
                        {
                            name: "rapid",
                            title: "Send requests rapidly (default count of 5)",
                            type: "boolean",
                        },
                        {
                            name: "tos",
                            title: "IP type-of-service value",
                            type: "number",
                            range: "0..255",
                        },
                        {
                            name: "ttl",
                            title: "IP time-to-live value (IPv6 hop-limit value)",
                            type: "number",
                            units: "hops",
                            range: "0..63",
                        },

                    ],
                },
            ],
        },
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
            },
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
            },
        });

        $("#target-input-form").submit(commandSubmit);
        $("#command-input-form").submit(commandSubmit);

        $("#input-enter").text("Enter").button({
            text: true,
        }).click(function (e) {
            commandSubmit(e);
            cmdHistory.focus();
            $(this).blur();
            return false;
        });

        var prefs_form = $.yform(prefs_options, "#prefs-dialog", prefs_fields);
        prefs = prefs_form.getData();

        $("#prefs").button().click(function (event) {
            $.dbgpr("prefsEdit:", event.type);
            if (prefs_form.shown()) {
                prefs_form.close();

                /* Put the focus back where it belongs */
                if (tgtHistory.value())
                    cmdHistory.focus();
                else tgtHistory.focus();

            } else {
                prefs_form.open();
            }
        });
    }

    function commandSubmit (event) {
        event.preventDefault();
        $.dbgpr("commandSubmit:", event.type);

        tgtHistory.close();
        cmdHistory.close();

        var command = cmdHistory.value();
        if (command == "") {
            $.dbgpr("submit; skipping since command value is empty");
            cmdHistory.focus();
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
                targetListMarkUsed(this, tname, cname);
        }

        cmdHistory.markUsed(command);
        commandOutputTrim(count);

        return false;
    }

    function targetListMarkUsed (form, target, cname) {
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
            targetListTrim($tset);
        }

        $target.click(function (event) {
            tgtHistory.close();
            tgtHistory.select(target);
            cmdHistory.focus();
        });

        $("#target-contents-none").css({ display: "none" });
    }

    function targetListTrim ($tset) {
        $(".target", $tset).each(function (index, target) {
            var delta = $(target).position().top -
                        $(target).parent().position().top;
            if ( delta > prefs.max_target_position) {
                $(target).remove();
            }
        });     
    }

    function commmandWrapperAdd (target, command) {
        var test;
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

        if (command.substring(0, 9) == "test form") {
            test = command.substring(10);
            if (testForms[test]) {
                content += "<div class='ui-yform'></div>";
            } else {
                test = undefined;
            }
            content += "<div class='output-replace' "
                + "style='white-space: pre-wrap'></div>";

        } else if (prefs.live_action) {
            content += "<div class='output-replace' "
                + "style='white-space: pre-wrap'>"
                + loadingMessage;
                + "</div>";

        } else {
            content += "<div class='output-replace' "
                + "style='white-space: pre-wrap'>"
                + "test-output\nmore\nand more and more\noutput\n";
                + "</div>";
        }

        content += "</div>";

        var $newp = jQuery(content);
        $output.prepend($newp);
        if (prefs.slide_speed > 0)
            $newp.slideUp(0).slideDown(prefs.slide_speed);
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

        } else if (prefs.live_action) {
            var $out = $("div.output-replace", $newp);
            $out.slideUp(0).slideDown(prefs.slide_speed);

            $out.load("/clira/clira.slax",
                         {
                             target: target,
                             command: command,
                             form: "false"
                         },
                         function (text, status, http) {
                             loadHttpReply(text, status, http,
                                           $(this), $out);
                         });

        }

        decorateIcons($newp);

        if (target) {
            $(".target-value", $newp).click(function () {
                tgtHistory.close();
                tgtHistory.select($(this).text());
                cmdHistory.focus();
            });
        }

        $(".command-value", $newp).click(function () {
            cmdHistory.close();
            cmdHistory.select($(this).text());
            cmdHistory.focus();
        });

        return false;
    }

    function loadHttpReply (text, status, http, $this, $out) {
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
        $out.slideDown(prefs.slide_speed);
    }

    function htmlEscape (val) {
        return val.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;");
    }

    function commandOutputTrimChanged () {
        commandOutputTrim(0);
    }

    function commandOutputTrim (fresh_count) {
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
            } else if (i >= prefs.output_remove_after + keep) {
                divRemove($child);
            } else if (i >= prefs.output_close_after + keep) {
                if (!divIsHidden($child))
                    divHide($child);
            } else {
                if (divIsHidden($child))
                    divUnhide($child);
            }
        }
    }

    function divRemove ($wrapper) {
        $wrapper.slideUp(prefs.slide_speed, function () {
            $wrapper.remove();
        });
    }

    function divIsHidden ($wrapper) {
        return $(".icon-hide-section", $wrapper).hasClass("hidden");
    }

    function divHide ($wrapper) {
        $(".icon-unhide-section", $wrapper).removeClass("hidden");
        $(".icon-hide-section", $wrapper).addClass("hidden");
        $(".can-hide", $wrapper).slideUp(prefs.slide_speed);
    }

    function divUnhide ($wrapper) {
        $(".icon-unhide-section", $wrapper).addClass("hidden");
        $(".icon-hide-section", $wrapper).removeClass("hidden");
        $(".can-hide", $wrapper).slideDown(prefs.slide_speed);
    }

    function prefsSetupConfirmExit () {
        if (prefs.stay_on_page) {
            window.onbeforeunload = function (e) {
                return "Are you sure?  You don't look sure.....";
            }
        } else {
            window.onbeforeunload = null;
        }
    }

    function prefsSwitchTheme (value, initial, prev) {
        if (initial)
            return;

        $("link.ui-theme, link.ui-addon").each(function () {
            var $this = $(this);
            var attr = $this.attr("href");
            attr = attr.replace(prev, value, "g");
            $this.attr("href", attr);
        });
    }

    /*
     * Our dbgpr container decorations (which need some of our functions)
     */
    function decorateIcons ($wrapper) {
        $(".icon-remove-section", $wrapper).text("Close").button({
            text: false,
            icons: { primary: "ui-icon-closethick" },
        }).click(function () {
            divRemove($wrapper);
        });

        $(".icon-hide-section", $wrapper).text("Hide").button({
            text: false,
            icons: { primary: "ui-icon-minusthick" },
        }).click(function () {
            divHide($wrapper);
        });

        $(".icon-unhide-section", $wrapper).text("Unhide").button({
            text: false,
            icons: { primary: "ui-icon-plusthick" },
        }).click(function () {
            divUnhide($wrapper);
        }).addClass("hidden");

        $(".icon-clear-section", $wrapper).text("Clear").button({
            text: false,
            icons: { primary: "ui-icon-trash" },
        }).click(function () {
            $(".can-hide", $wrapper).text("");
        });

        $(".icon-keeper-section", $wrapper).text("Keep").button({
            text: false,
            icons: { primary: "ui-icon-star" },
        }).click(function () {
            $wrapper.toggleClass("keeper-active");
            $(this).toggleClass("ui-state-highlight");
        });
    }

    function hideCommandForm (yform) {
        var $top = $(yform.form).parents("div.output-wrapper");
        var $bar = $("div.output-header", $top);

        if ($("button.icon-show-form", $bar).length == 0) {
            $bar.append("<button class='icon-show-form'/>"
                        + "<button class='icon-hide-form'/>");

            $(".icon-show-form", $bar).text("Show Form").button({
                text: false,
                icons: { primary: "ui-icon-circle-arrow-s" },
            }).click(function () {
                $(".icon-hide-form", $bar).removeClass("hidden");
                $(".icon-show-form", $bar).addClass("hidden");
                yform.form.slideDown(prefs.slide_speed);
            });

            $(".icon-hide-form", $bar).text("Hide Form").button({
                text: false,
                icons: { primary: "ui-icon-circle-arrow-n" },
            }).addClass("hidden").click(function () {
                $(".icon-show-form", $bar).removeClass("hidden");
                $(".icon-hide-form", $bar).addClass("hidden");
                yform.form.slideUp(prefs.slide_speed);
            });
        }

        $(".icon-show-form", $bar).removeClass("hidden");
        $(".icon-hide-form", $bar).addClass("hidden");
        yform.form.slideUp(prefs.slide_speed);
    }

    function runRpc (yform, $out, rpc, target) {
        $.dbgpr("runrpc:", rpc);
        if (prefs.live_action) {
            $out.slideUp(0).slideDown(prefs.slide_speed);
            $out.load("/clira/clira.slax",
                         {
                             target: target, // target is optional
                             rpc: rpc,       // rpc is in string form
                             form: "false",  // don't want form in reply
                         },
                         function (text, status, http) {
                             loadHttpReply(text, status, http, $(this), $out);
                         });
        } else {
            $out.text("RPC:\n" + rpc);
        }
    }

    cliInit();
    tgtHistory.focus();
});
