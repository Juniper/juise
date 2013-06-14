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
        name: "debug",
        commands: [
            {
                command: "enable debugging",
                help: "Log debug messages",
                execute: function ($output, cmd, parse, poss) {
                    localStorage['debug'] = true;
                    $("#debug-container").css({ display: "inline" });
                    $output.html("Enabled debugging");
                },
            },
            {
                command: "disable debugging",
                help: "Disable logging of debug messages",
                execute: function ($output, cmd, parse, poss) {
                    localStorage['debug'] = false;
                    $("#debug-log").empty();
                    $("#debug-container").css({ display: "none" });
                    $output.html("Disabled debugging");
                },
            },
            {
                command: "pause debugging",
                help: "Pause logging of debug messages",
                execute: function ($output, cmd, parse, poss) {
                    localStorage['debug'] = false;
                    $output.html("Paused debugging");
                },
            },
            {
                command: "resume debugging",
                help: "Enable/resume logging of debug messages",
                execute: function ($output, cmd, parse, poss) {
                    localStorage['debug'] = true;
                    $("#debug-container").css({ display: "inline" });
                    $output.html("Resumed debugging");
                },
            },
        ],
    });
});
