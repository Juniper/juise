/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../../../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function($) {
    jQuery.clira.commandFile({
        name: "history",
        commands: [
            {
                command: "show command history",
                help: "Display list of all the commands executed along with "
                    + "timestamp",
                execute: function ($output, cmd, parse, poss) {
                    var history = $.clira.cmdHistory.show(),
                        html = '';
                    for (var i = history.length - 1; i >= 0; i--) {
                        html += "<div class='history-element'><span "
                              + "class='command'>" + history[i]['command'] 
                              + "</span> - <span class='date'>"
                              + new Date(history[i]['on']) + "</span></div>";
                    }
                    $output.html(html);
                },
            },
            {
                command: "clear command history",
                help: "Clears history of executed commands",
                execute: function ($output, cmd, parse, poss) {
                    $.clira.cmdHistory.clear();
                    $output.html("Cleared command history");
                },
            },
        ],
    });
});
