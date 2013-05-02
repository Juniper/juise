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

    var loadingMessage = "<img src='/icons/loading.png'>"
        + "    ....loading...\n";

    if ($.clira == undefined)
        $.clira = { };

    $.extend($.clira, {
        prefs: { },
        prefsInit: function prefsInit () {
            $.clira.prefs_form = $.yform(prefs_options, "#prefs-dialog",
                                         prefs_fields);
            $.clira.prefs = $.clira.prefs_form.getData();
            $("#prefs").button().click(function (event) {
                $.dbgpr("prefsEdit:", event.type);
                if ($.clira.prefs_form.shown()) {
                    $.clira.prefs_form.close();

                    /* Put the focus back where it belongs */
                    $.clira.refocus();

                } else {
                    $.clira.prefs_form.open();
                }
            });

            $("#prefs-main-form").dialog({
                autoOpen: false,
                height: 300,
                width: 350,
                modal: true,
                buttons: {
                    'Close': function() {
                        $(this).dialog("close");
                    }
                },
                close: function() {
                },
            });

            $("#prefsbtn").click(function() {
                $("#prefs-main-form").dialog("open");
            });

            /* Set Up Devices */
            $("#prefs-devices-form").dialog({
                autoOpen: false,
                height: 600,
                width: 800,
                resizable: false,
                modal: true,
                buttons: {
                    'Close': function() {
                        $(this).dialog("close");
                    }
                },
                close: function() {
                },
            });

            $("#prefs-devices").click(function() {
                $("#prefs-devices-form").dialog("open");

                $("#prefs-devices-grid").jqGrid({
                    url: '/clira/db.php?p=device_list',
                    editurl: '/clira/db.php?p=device_edit',
                    datatype: 'json',
                    colNames: ['Name', 'Hostname', 'Port',
                               'Username', 'Password', 'Save Password', ''],
                    colModel: [
                        {
                            name: 'name',
                            index: 'index',
                            width: 90,
                            editable: true,
                            editrules: {
                                required: true
                            },
                        },
                        {
                            name: 'hostname',
                            index: 'hostname',
                            width: 100,
                            editable: true,
                            editrules: {
                                required: true
                            },
                        },
                        {
                            name: 'port',
                            index: 'port',
                            width: 50,
                            editable: true
                        },
                        {
                            name: 'username',
                            index: 'username',
                            width: 100,
                            editable: true,
                            editrules: {
                                required: true
                            },
                        },
                        {
                            name: 'password',
                            index: 'password',
                            width: 40,
                            editable: true,
                            edittype: 'password',
                            hidden: true,
                            hidedlg: true,
                            editrules: {
                                edithidden: true,
                            },
                        },
                        {
                            name: 'save_password',
                            index: 'save_password',
                            width: 20,
                            editable: true,
                            edittype: 'checkbox',
                            editoptions: {
                                value: 'yes:no',
                                defaultValue: 'no',
                            },
                            formatter: 'checkbox',
                        },
                        {
                            name: 'action',
                            index: 'action',
                            width: 40,
                            formatter: 'actions',
                            formatoptions: {
                                editformbutton: true,
                                editOptions: {
                                    closeAfterEdit: true,
                                    afterShowForm: function ($form) {
                                        var $dialog = $('#editmodprefs-devices-grid');
                                        var grid = $('#prefs-devices-grid');
                                        var coord = {};

                                        coord.top = grid.offset().top + (grid.height() / 2);
                                        coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);

                                        $dialog.offset(coord);
                                    }
                                },
                                delOptions: {
                                    afterShowForm: function ($form) {
                                        var $dialog = $form.closest('div.ui-jqdialog');
                                        var grid = $('#prefs-devices-grid');
                                        var coord = {};

                                        coord.top = grid.offset().top + (grid.height() / 2);
                                        coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);

                                        $dialog.offset(coord);
                                    }
                                },
                            },
                        },
                    ],
                    rowNum: 10,
                    sortname: 'name',
                    autowidth: true,
                    viewrecords: true,
                    sortorder: 'asc',
                    height: 400,
                    pager: '#prefs-devices-pager',
                    beforeSelectRow: function (rowid, e) {
                        if (e.target.id == 'delete') {
                            // Do our row delete
                            alert('delete row ' + rowid);
                        }
                    },
                }).navGrid('#prefs-devices-pager', {
                    edit:false,
                    add:true,
                    del:false,
                    search:false,
                }, {
                    //prmEdit
                    closeAfterEdit: true
                }, {
                    //prmAdd,
                    closeAfterAdd: true
                });
            });
        
            /* Set Up Groups */
            $("#prefs-groups-form").dialog({
                autoOpen: false,
                height: 600,
                width: 800,
                resizable: false,
                modal: true,
                buttons: {
                    'Close': function() {
                        $(this).dialog("close");
                    }
                },
                close: function() {
                }
            });

            $("#prefs-groups").click(function() {
                $("#prefs-groups-form").dialog("open");

                $("#prefs-groups-grid").jqGrid({
                    url: '/clira/db.php?p=group_list',
                    editurl: '/clira/db.php?p=group_edit',
                    datatype: 'json',
                    colNames: ['Name', 'Members', ''],
                    colModel: [
                        {
                            name: 'name',
                            index: 'name',
                            width: 90,
                            editable: true,
                            editrules: {
                                required: true
                            },
                        },
                        {
                            name: 'members',
                            index: 'devices',
                            editable: true,
                            edittype: 'select',
                            editrules: {
                                required: true,
                            },
                            editoptions: {
                                multiple: true,
                                dataUrl: '/clira/db.php?p=device',
                                buildSelect: function (data) {
                                    var j = $.parseJSON(data);
                                    var s = '<select>';
                                    if (j.devices && j.devices.length) {
                                        $.each(j.devices, function (i, item) {
                                            s += '<option value="' + item.id + '">' + item.name + '</option>';
                                        });
                                    }
                                    return $(s)[0];
                                },
                            },
                        },
                        {
                            name: 'action',
                            index: 'action',
                            width: 40,
                            formatter: 'actions',
                            formatoptions: {
                                editformbutton: true,
                                editOptions: {
                                    closeAfterEdit: true,
                                    afterShowForm: function ($form) {
                                        var $dialog = $('#editmodprefs-groups-grid');
                                        var grid = $('#prefs-groups-grid');
                                        var coord = {};

                                        coord.top = grid.offset().top + (grid.height() / 2);
                                        coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);

                                        $dialog.offset(coord);
                                    }
                                },
                                delOptions: {
                                    afterShowForm: function ($form) {
                                        var $dialog = $form.closest('div.ui-jqdialog');
                                        var grid = $('#prefs-groups-grid');
                                        var coord = {};

                                        coord.top = grid.offset().top + (grid.height() / 2);
                                        coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);

                                        $dialog.offset(coord);
                                    }
                                },
                            },
                        },
                    ],
                    rowNum: 10,
                    sortname: 'name',
                    autowidth: true,
                    viewrecords: true,
                    sortorder: 'asc',
                    height: 400,
                    pager: '#prefs-groups-pager',
                    beforeSelectRow: function (rowid, e) {
                        if (e.target.id == 'delete') {
                            // Do our row delete
                            alert('delete row ' + rowid);
                        }
                    },
                }).navGrid('#prefs-groups-pager', {
                    edit:false,
                    add:true,
                    del:false,
                    search:false,
                }, {
                    //prmEdit
                    closeAfterEdit: true
                }, {
                    //prmAdd,
                    closeAfterAdd: true
                });
            });

            $.extend(jQuery.jgrid.edit, { recreateForm: true });
       
            /* General Preferences */
            $("#prefs-general").click(function() {
                //$("#prefs-general-form").dialog("open");
                // XXX: rkj    use new forms, decide what prefs we need?
                if ($.clira.prefs_form.shown()) {
                    $.clira.prefs_form.close();
                    
                    if (tgtHistory.value())
                        cmdHistory.focus();
                    else tgtHistory.focus();

                } else {
                    $.clira.prefs_form.open();
                }
            });
        
            $("#prefs-general-form").dialog({
                autoOpen: false,
                height: 600,
                width: 800,
                resizable: false,
                modal: true,
                buttons: {
                    'Close': function() {
                        $(this).dialog("close");
                    }
                },
                close: function() {
                }
            });
        },
        decorateIcons: function decorateIcons ($wrapper) {
            // Our container decorations (which need some of our functions)
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
        },

        makeAlert: function makeAlert ($div, message, defmessage) {
            if (message == undefined || message.length == 0)
                message = defmessage;
            var content = '<div class="ui-state-error ui-corner-all">'
                + '<span><span class="ui-icon ui-icon-alert">'
                + '</span>';
            content += '<strong>Error:</strong> ' + message + '</span></div>';
            $div.html(content);
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
                        if ($.clira.cmdHistory)
                            $.clira.cmdHistory.close();
                    },
                });
            }

            $("#target-input-form").submit(submit);
            $("#command-input-form").submit(submit);

            $("#input-enter").text("Enter").button({
                text: true,
            }).click(function (e) {
                submit(e);
                $.clira.cmdHistory.focus();
                $(this).blur();
                return false;
            });

            $("#juise").button();
        },
    });

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
            def: true,
            type: "boolean",
            label: "Live",
            title: "Interact with real devices",
        },
        {
            name: "mixer",
            def: "ws://127.0.0.1:3000/mixer",
            type: "string",
            label: "Mixer Location",
            title: "Address of the Mixer server",
            change: prefsChangeMuxer,
        },
    ];

    var prefs_options = {
        preferences: {
            apply: function () {
                $.dbgpr("prefsApply");
                $.clira.prefs_form.close();
                $.clira.prefs = $.clira.prefs_form.getData();
            }
        },
        title: "Preferences",
        dialog : { },
    }

    $.dbgpr("document is ready");

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
                targetListMarkUsed(this, tname, cname);
        }

        $.clira.cmdHistory.markUsed(command);
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
            $.clira.cmdHistory.focus();
        });

        $("#target-contents-none").css({ display: "none" });
    }

    function targetListTrim ($tset) {
        $(".target", $tset).each(function (index, target) {
            var delta = $(target).position().top -
                        $(target).parent().position().top;
            if ( delta > $.clira.prefs.max_target_position) {
                $(target).remove();
            }
        });     
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
        runCommand: function runCommand ($output, target, command) {
            $output.html(loadingMessage);
            $output.slideUp(0).slideDown($.clira.prefs.slide_speed);

            if (muxer == undefined)
                openMuxer();

            var full = [ ];

            muxer.rpc({
                div: $output,
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
                    if (full.length <= 2)
                        $output.html(data);
                },
                oncomplete: function () {
                    $.dbgpr("rpc: complete");
                    $output.html(full.join(""));
                },
                onhostkey: function (data) {
                    var self = this;
                    promptForHostKey($output, data, function (response) {
                        muxer.hostkey(self, response);
                    });
                },
                onpsphrase: function (data) {
                    var self = this;
                    promptForSecret($output, data, function (response) {
                        muxer.psphrase(self, response);
                    });
                },
                onpsword: function (data) {
                    var self = this;
                    promptForSecret($output, data, function (response) {
                        muxer.psword(self, response);
                    });
                },
                onclose: function (event, message) {
                    $.dbgpr("muxer: rpc onclose");
                    if (full.length == 0) {
                        $.clira.makeAlert($output, message,
                                          "internal failure (websocket)");
                    }
                },
                onerror: function (message) {
                    $.dbgpr("muxer: rpc onerror");
                    if (full.length == 0) {
                        $.clira.makeAlert($output, message,
                                          "internal failure (websocket)");
                    }
                },
            });
        },
    });

    function promptForHostKey ($parent, prompt, onclick) {
        var content = "<div class='muxer-prompt ui-state-highlight'>"
            + "<div class='muxer-message'>" + prompt + "</div>"
            + "<div class='muxer-buttons'>" 
            +   "<button class='accept'/>"
            +   "<button class='decline'/>"
            + "</div></div>";

        var $div = $(content);
        $parent.html($div);
        $(".accept", $div).text("Accept").button({}).focus().click(function () {
            onclick("yes");
            restoreReplaceDiv($div);
        });
        $(".decline", $div).text("Decline").button({}).click(function () {
            onclick("no");
            restoreReplaceDiv($div);
        });
    }

    function promptForSecret ($parent, prompt, onclick) {
        var content = "<div class='muxer-prompt ui-state-highlight'>"
            + "<div class='muxer-message'>" + prompt + "</div>"
            + "<form action='#' class='form'>"
            +   "<input name='value' type='password' class='value'></input>"
            +   "<input type='submit' class='submit' hidden='yes'></input>"
            + "</form>"
            + "<div class='muxer-buttons'>" 
            +   "<button class='enter'/>"
            +   "<button class='cancel'/>"
            + "</div></div>";

        var $div = $(content);
        $parent.html($div);
        $(".form", $div).submit( function () {
            var val = $(".value", $div).val();
            onclick(val);
            restoreReplaceDiv($div);
        }).children("input.value").focus();
        $(".enter", $div).text("Enter").button({}).click(function () {
            var val = $(".value", $div).val();
            onclick(val);
            restoreReplaceDiv($div);
        });
        $(".cancel", $div).text("Cancel").button({}).click(function () {
            var val = $(".value", $div).val();
            onclick(val);
            $div.remove();
            $parent.html(loadingMessage);
        });
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

    function prefsSetupConfirmExit () {
        if ($.clira.prefs.stay_on_page) {
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

    function prefsChangeMuxer (value, initial, prev) {
        if (initial)
            return;

        if (muxer) {
            muxer.close();
            muxer = undefined;
        }
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
                yform.form.slideDown($.clira.prefs.slide_speed);
            });

            $(".icon-hide-form", $bar).text("Hide Form").button({
                text: false,
                icons: { primary: "ui-icon-circle-arrow-n" },
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
                             form: "false",  // don't want form in reply
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
