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
    if ($.clira == undefined)
        $.clira = { };

    var prefs_fields = [
        {
            name: "output_close_after",
            def: 5,
            type: "number",
            change: $.clira.commandOutputTrimChanged,
            title: "Number of open commands"
        },
        {
            name: "output_remove_after",
            def: 10,
            type: "number",
            change: $.clira.commandOutputTrimChanged,
            title: "Total number of commands"
        },
        {
            name: "slide_speed",
            def: 0, // "fast",
            type: "number-plus",
            title: "Speed of slide animations"
        },
        {
            name: "max_commands_list",
            def: 40,
            type: "number",
            title: "Number of commands kept in list"
        },
        {
            name: "max_targets_inline",
            def: 10,
            type: "number",
            title: "Number of targets kept on screen"
        },
        {
            name: "max_targets_list",
            def: 40,
            type: "number",
            title: "Number of targets kept in list"
        },
        {
            name: "max_target_position",
            def: 10,
            type: "number",
            title: "Target button rows"
        },
        {
            name: "stay_on_page",
            def: false,
            type: "boolean",
            label: "Stay",
            change: prefsSetupConfirmExit,
            title: "Give warning if leaving this page"
        },
        {
            name: "theme",
            def: "black-tie",
            type: "string",
            title: "UI Theme",
            change: prefsSwitchTheme
        },
        {
            name: "live_action",
            def: true,
            type: "boolean",
            label: "Live",
            title: "Interact with real devices"
        },
        {
            name: "mixer",
            def: "ws://127.0.0.1:3000/mixer",
            type: "string",
            label: "Mixer Location",
            title: "Address of the Mixer server",
            change: $.clira.prefsChangeMuxer
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
        dialog : { }
    }

    $.extend($.clira, {
        prefs: { },
        prefsInit: function prefsInit () {
            for (var i = 0; i < prefs_fields.length; i++) {
                // Save the preference item if not already saved
                var pref = Clira.Preference.find(prefs_fields[i]['name']);
                if (Ember.isNone(pref)) {
                    var item = prefs_fields[i];

                    // We add a new field to hold the configured value
                    item['value'] = item['def'];

                    // Create and save as record
                    pref = Clira.Preference.create(item);
                    pref.saveRecord();
                }
            }

            // Read preferences into $.clira.prefs to they can be used
            // elsewhere
            var prefs = Clira.Preference.findAll();
            if (prefs) {
                prefs.forEach(function(item) {
                    $.clira.prefs[item.get('name')] = item.get('value');
                });
            }
        },
        buildPrefForms: function() {
            return buildForms();
        }
    });

    function buildForms () {
        $.extend(jQuery.jgrid.edit, { recreateForm: true });
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
});
