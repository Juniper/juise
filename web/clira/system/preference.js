/*
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2014, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../../../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function($) {
    jQuery.clira.commandFile({
        name: "preferences",
        commands: [
            {
                command: "edit preferences",
                help: "View and change CLIRA settings",
                execute: function (view, cmd, parse, poss) {
                    view.createChildView(Clira.PreferencesDialog).append();
                }
            }
        ]
    });
});
