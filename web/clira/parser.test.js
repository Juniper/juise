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
                appendTo: $("#command-input-box"),
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

                            //
                            // If the command defines a custom completion,
                            // use it.
                            //
                            if (false && p.command.complete) {
                                $.dbgpr("calling custom completion");
                                p.command.complete(p, res, value);
                            }

                        }
                    });
                    response(res);
                },
                open: function () {
                    $inputbox = $("#command-input-box");
                    var position = $inputbox.position(),
                        left = position.left,
                        top = position.top;

                    $("#command-input-box > ul").css({
                        left: left + "px", width: $inputbox.width() + "px",
                        top: top + $inputbox.parent().height() + "px"
                    });
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
            $.clira.cmdHistory.record(command);
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
                $.clira.makeAlert($div, "unknown command");
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

    $.clira.loadCommandFiles();

    if (false) {
        runParsingTests();
        runCommandTests();
    }

    $.clira.initInput();
});

