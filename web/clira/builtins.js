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
    $.clira.loadBuiltins = function () {
        $.clira.addCommand([
            {
                command: "show commands",
                help: "Display a list of all available commands currently "
                    + "loaded in CLIRA",
                execute: function ($output, cmd, parse, poss) {
                    $.each($.clira.commands, function (n, c) {
                        var html = "<div>" + c.command + "</div>";
                        $output.append(html);
                    });
                },
            },
            {
                command: "reload commands",
                help: "Reload CLIRA command set",
                execute: function ($output, cmd, parse, poss) {
                    $output.text("Reloading commands");
                    $.clira.loadCommandFiles();
                },
            },
        ]);
    }
});
