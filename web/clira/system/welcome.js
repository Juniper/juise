/*
 * Copyright 2014, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../../../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function($) {
    jQuery.clira.commandFile({
        name: "welcome",
        templatesFile: '/clira/templates/welcome.hbs',
        commands: [
            {
                command: "show welcome screen",
                help: "Displays start up screen",
                templateName: "show-welcome",
                execute: function (view, cmd, parse, poss) {
                }
            }
        ]
    });
});
