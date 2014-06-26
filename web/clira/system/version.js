/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2014, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    jQuery.clira.commandFile({
        name: "version",
        commands: [
            {
                command: "version",
                arguments: [{
                    name: "target",
                    type: "string",
                    help: "Remote device name",
                    nokeyword: true
                }],
                help: "Show Junos version on a device",
                execute: junosVersion
            }
        ]
    });

    function junosVersion (view, cmd, parse, poss) {
        if (!poss.data.target) {
            $.clira.makeAlert(view, "You must include a target "
                + "for the 'version' command");
            return;
        }

        view.get('controller').set('output', "Connecting...");

        $.clira.muxer();

        $.clira.runSlax({
            script: '/clira/system/version.slax',
            args: {
                target: poss.data.target
            },
            view: view,
            success: function (data) {
                view.get('controller').set('output', data);
            },
            failure: function (data) {
                $.clira.makeAlert(view, "Error executing command: " + data);
            }
        });
    }
});
