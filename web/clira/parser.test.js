/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    var command_number = 0;

    function addTestCommands () {
        $.clira.addCommand([
            {
                command: "show interfaces",
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name",
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type",
                    },
                    {
                        name: "statistics",
                        type: "empty",
                        help: "Show statistics only",
                    },
                    {
                        name: "color",
                        type: "enumeration",
                        help: "Color of interface",
                        enums: [
                            {
                                name: "blue",
                                help: "Blue like the sea after a storm",
                            },
                            {
                                name: "black",
                                help: "Black line the night",
                            },
                            {
                                name: "red",
                                help: "Red",
                            }
                        ],
                    },
                ],
                execute: function () {
                    $.dbgpr("got it");
                },
            },
            {
                command: "show alarms",
                bundle: [ "affecting", "since", "location", ],
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name",
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type",
                    },
                ],
                execute: function () {
                    $.dbgpr("got it");
                },
            },
            {
                command: "show alarms critical extensive",
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name",
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type",
                    },
                ],
                execute: function () {
                    $.dbgpr("got it");
                },
            },
            {
                command: "tell",
                arguments: [
                    {
                        name: "user",
                        type: "string",
                        help: "User to send message to",
                        nokeyword: true,
                        mandatory: true,
                    },
                    {
                        name: "message",
                        type: "string",
                        multiple_words: true,
                        help: "Message to send to user",
                        nokeyword: true,
                        mandatory: true,
                    },
                ],
                execute: function ($output, cmd, parse, poss) {
                    $.dbgpr("got it");
                    $output.html("<div><span>Executing </span>"
                                 + poss.html + "<span> ...</span></div>");
                },
            },
            {
                command: "on",
                arguments: [
                    {
                        name: "target",
                        type: "string",
                        help: "Remote device name",
                        nokeyword: true,
                    },
                    {
                        name: "command",
                        type: "string",
                        multiple_words: true,
                        help: "Command to execute",
                        nokeyword: true,
                    },
                ],
                execute: function ($output, cmd, parse, poss) {
                    parse.dbgpr("working command: " + poss.command.command);
                    $output.html("<div> tada </div>");
                    $.clira.targetListMarkUsed(poss.data.target,
                                               poss.data.target,
                                               function ($target, target) {
                                                   $.clira.cmdHistory.select("on " + target + " ");
                                               });
                    $.clira.runCommand($output, poss.data.target,
                                       poss.data.command);
                },
            },
            {
                command: "show outages",
                bundle: [ "location", "since", ],
            },
            {
                command: "map outages",
                bundle: [ "affecting", "since", ],
            },
            {
                command: "list outages",
                bundle: [ "affecting", "since", "between-locations", ],
            },
            {
                command: "list flaps",
                bundle: [ "affecting", "since", "between-locations", ],
            },
            {
                command: "show latency issues",
                bundle: [ "affecting", "since", "location" ],
            },
            {
                command: "show drop issues",
                bundle: [ "affecting", "since", ],
            },
            {
                command: "map paths",
                bundle: [ "between-locations", ],
            },
            {
                command: "list flags",
                bundle: [ "affecting", "since",
                          "between-locations", "location", ],
            },
            {
                command: "test lsp",
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true,
                    },
                ],
            },
            {
                command: "show fancy box",
                execute: function fancyBox ($output, cmd, parse, poss) {
                    var svg = "<svg xmlns='http://www.w3.org/2000/svg'\
    xmlns:xlink='http://www.w3.org/1999/xlink'>\
    \
    <rect x='10' y='10' height='110' width='110'\
         style='stroke:#ff0000; fill: #0000ff'>\
    \
        <animateTransform\
            attributeName='transform'\
            begin='0s'\
            dur='20s'\
            type='rotate'\
            from='0 60 60'\
            to='360 60 60'\
            repeatCount='indefinite' \
        />\
    </rect>\
\
</svg>\
";
                    $output.html(svg);
                },
            },
            {
                command: "route lsp away from device",
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true,
                    },
                    {
                        name: "device-name",
                        type: "device",
                        nokeyword: true,
                    },
                ],
            },
            {
                command: "configure new lsp",
                bundle: [ "between-devices", ],
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true,
                    },
                ],
            },
            {
                command: "add device to vpn",
                arguments: [
                    {
                        name: "device-name",
                        type: "device",
                        nokeyword: true,
                    },
                    {
                        name: "interface",
                        type: "interface",
                    },
                    {
                        name: "vpn-name",
                        type: "vpn",
                    },
                ],
                execute: function ($output, cmd, parse, poss) {
                },
            },
            {
                command: "run parse tests",
                execute: function ($output, cmd, parse, poss) {
                    runParsingTests($output);
                },
            },
        ]);
    }

    function runParsingTests ($wrapper) {

        if ($wrapper == undefined)
            $wrapper = $("#output-top");

        $.each(
            [
                "te",
                "tell",
                "tell ",
                "show inter color red",
                "show interfaces color bl",
                "show alarms",
                "alarms ext",
                "complete failure",
                "show al c e",
                "show interfaces type ethernet statistics",
                "show interfaces statistics",
                "tell user phil message now is the time",
                "tell phil this one is working",
                "tell user phil message user security must work",
                "on dent show interfaces fe-0/0/0",
                "show latency issues near iad",
                "show outages near iad",
                "list outages since yesterday",
                "list outages between lax and bos",
                "map paths between lax and bos",
                "list flaps near lax since yesterday",
                "list flaps between device lax and location boston",
                "test lsp foobar",
                "show alarms for northeast",
                "configure new lsp goober between bos and lax",
                "add device bos interface fe-0/0/0 to vpn corporate",
                "route lsp foobar away from device bos",
                "map outages affecting lsp foobar",
                "show latency issues affecting lsp foobar",
                "show drop issues affecting customer blah",
            ], function (x, cmd) {
                $.dbgpr("test: input: [" + cmd + "]");
                var res = $.clira.parse(cmd);
                $.dbgpr("res: " + res.possibilities.length);

                var html = res.render({details: true});
                var $out = $(html);
                $wrapper.append($out);
            }
        );
    }

    function runCommandTests () {
        $.each(
            [
                "tell phil this is working",
            ], function (x, cmd) { $.clira.executeCommand(cmd); }
        );
    }

    $.extend($.clira, {
        initInput: function initInput () {
            $.clira.cliraInit(false, $.clira.commandSubmit);

            $.clira.cmdHistory = $.mruPulldown({
                name: "command-history",
                top: $("#command-top"),
                clearIcon: $("#command-clear"),
                entryClass: "command-history-entry",
            });

            $command = $("#command");
            $command.focus();
            $command.autocomplete({
                source: function (value, response) {
                    $.dbgpr("test: input: [" + value.term + "]");
                    var parse = $.clira.parse(value.term);
                    $.dbgpr("parse: " + parse.possibilities.length);

                    var res = [ ];

                    parse.eachPossibility(function (n, p) {
                        p.render();
                        if (p.text) {
                            var r = {
                                label: p.text,
                                value: p.text,
                                html:  p.html,
                                help: p.command.help,
                                image: p.command.image,
                                image_class: p.command.image_class,
                            }

                            res.push(r);
                        }
                    });
                    response(res);
                },
            }).data('autocomplete')._renderItem = $.clira.renderItemOverride;

            $("#command-input-form").submit($.clira.commandSubmit);
        },

        commandSubmit: function commandSubmit (event) {
            event.preventDefault();
            $.clira.cmdHistory.close();

            var command = $.clira.cmdHistory.value();
            if (command == "") {
                $.dbgpr("submit; skipping since command value is empty");
                $.clira.cmdHistory.focus();
                return false;
            }

            $.clira.cmdHistory.select("");
            $("#command").data("autocomplete").close();
            $.clira.executeCommand(command);

            $.clira.cmdHistory.markUsed(command);
            $.clira.commandOutputTrim(1);
        },

        buildWrapper: function buildWrapper (cmd, html) {
            var content = "<div class='output-wrapper ui-widget "
                +     "ui-widget-content ui-corner-all'>"
                + "<div class='output-header ui-state-default "
                +     "ui-widget-header ui-corner-all'>"
                + "<button class='icon-remove-section'></button>"
                + "<button class='icon-hide-section'></button>"
                + "<button class='icon-unhide-section'></button>"
                + "<button class='keeper icon-keeper-section'></button>"
                + "<div class='target-value-empty' style='display: none'>"
                + "</div>"
                + "<div class='command-value rounded blue'>"
                + cmd
                + "</div><div class='command-number'>"
                + " (" + ++command_number + ")"
                + "</div></div>"
                + "<div class='output-content can-hide'>"
                + html
                + "<div class='output-replace'></div></div>";

            return content;
        },

        executeCommand: function executeCommand (cmd) {
            $.dbgpr("execute: input: [" + cmd + "]");
            var parse = $.clira.parse(cmd);
            $.dbgpr("parse: " + parse.possibilities.length);

            var html = parse.render();

            var content = $.clira.buildWrapper(cmd, html);

            var $newp = $(content);
            $.clira.decorateIcons($newp);
            $("#output-top").prepend($newp);
            var $output = $("div.output-replace", $newp);
            $output.slideUp(0).slideDown($.clira.prefs.slide_speed);

            var poss = parse.possibilities[0];

            if ($.clira.emitParseErrors(parse, poss, $output))
                return;

            parse.dbgpr("calling execute function for '"
                      + poss.command.command + "' ...");
            poss.command.execute.call(poss.command, $output, cmd, parse, poss);
            parse.dbgpr("return from execute");
        },
        
        emitParseErrors: function emitParseErrors(parse, poss, $output) {
            var res = false;

            if (poss == undefined) {
                var $div = $("<div class='parse-error'></div>")
                    .appendTo($output);
                $.clira.makeAlert($div, "command parse failure");
                return true;
            }

            $.each(poss.command.arguments, function (n, arg) {
                var message;

                if (arg.mandatory && !poss.seen[arg.name]) {
                    message = "Missing mandatory argument: " + arg.name;
                } else if ($.clira.types[arg.type].needs_data
                        && poss.seen[arg.name] && !poss.data[arg.name]) {
                    message = "Missing argument value: " + arg.name;
                }

                if (message) {
                    var $div = $("<div class='parse-error'></div>")
                        .appendTo($output);
                    $.clira.makeAlert($div, message, "parse error");
                    res = true;
                }
            });

            return res;
        },

        renderItemOverride: function renderItemOverride(ul, item) {
            var append = "<a class='>";

            if (item.image) {
                append += "<img src='" + item.image + "' class='rendered";
                if (item.image_class)
                    append += " " + item.image_class;
                append += "'></img>";
            } else if (item.image_class)
                append += "<div class='" + item.image_class + "'></div>";

            append += "<div>" + item.html;
            if (item.help)
                append += "</div><div class='command-help'>" + item.help;
            append += "</div></a>";

            return $("<li></li>")
                .data("item.autocomplete", item)
                .append(append)
                .appendTo(ul);
        },
    });

    addTestCommands();
    if (false) {
        runParsingTests();
        runCommandTests();
    }

    $.clira.initInput();
});

