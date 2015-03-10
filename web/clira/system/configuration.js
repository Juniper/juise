/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2015, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    Configuration = Em.Namespace.create();

    Configuration.CommitButton = JQ.ButtonView.extend({
        label: 'Commit',
        enabled: true,

        resetButton: function () {
            var button = this.$().closest('.output-content').find('div#buttonbar div button.ui-button span');

            if (this.enabled) {
                button.html('Commit')
            } else {
                button.html('Working...');
            }
        },

        click: function() {
            var parentThis = this;

            if (!this.enabled) {
                return;
            }

            var output = this.$().closest('.output-content');

            var config = output.children('#pastebox').val().trim();
            var checkOnly = output.find('#checkOnly').is(':checked');
            var loadStyle = output.find('#loadStyle').val();
            var action = 'merge';
            var format = 'xml';
            var commitOutput = output.find('.commit-output');
            commitOutput.uniqueId();

            if (config.length == 0) {
                commitOutput.html('<b>Please paste a configuration snippet to load.</b>');
                return;
            }

            commitOutput.html('working....');

            switch (loadStyle) {
            case 'load merge': action = 'merge'; break;
            case 'load update': action = 'update'; break;
            case 'load override': action = 'override'; break;
            case 'load replace': action = 'replace'; break;
            case 'load set': action = 'set'; format = 'text'; break;
            case 'load patch': action = 'patch'; foramt = 'text'; break;
            }

            this.enabled = false;
            this.resetButton();

            $.clira.loadConfig(Configuration.Target, config, function (success, output) {
                var xml = $.parseXML(output);
                if (!success) {
                    var errorMessage = $('<div/>').text($(xml).find('error-message').text()).html();
                    var badElement = $('<div/>').text($(xml).find('bad-element').text()).html();

                    var message = 'There was an error loading this configuration: <b>'
                        + errorMessage + '</b>';
                    if (badElement.trim().length > 0) {
                        message += ' at element <b>' + badElement + '</b>';
                    }
                    commitOutput.html(message);
                    parentThis.enabled = true;
                    parentThis.resetButton();
                } else {
                    var ok = $(xml).find('ok');
                    if (ok) {
                        // We've successfully loaded the configuration, lets
                        // go ahead and commit it
                        $.clira.commitConfig(Configuration.Target, checkOnly, function (success, output) {
                            if (!success) {
                                commitOutput.html('<b>Could not commit configuration!</b>');
                                parentThis.enabled = true;
                                parentThis.resetButton();
                            } else {
                                if (checkOnly) {
                                    // Roll back our configuration
                                    $.clira.rollbackConfig(Configuration.Target, 0, function (success, output) {
                                        if (success) {
                                            commitOutput.html('<b>Configuration was loaded and checked successfully.  The configuration has not been modified.</b>');
                                        } else {
                                            commitOutput.html('<b>Could not roll back configuration!</b>');
                                        }
                                        parentThis.enabled = true;
                                        parentThis.resetButton();
                                    });
                                } else {
                                    commitOutput.html('<b>Configuration was successfully loaded and committed!</b>');
                                    parentThis.enabled = true;
                                    parentThis.resetButton();
                                }
                            }
                        }, Configuration.View);
                    }
                }
            }, format, action, Configuration.View);
        }
    });

    Configuration.TextArea = Ember.TextArea.extend({
        lastMode: 'TEXT',

        didInsertElement: function () {
            Ember.run.next(function () {
                this.$().focus();
                this.$().on('keyup', function (e) {
                    var type = 'NONE';

                    // Determine what kind of input we have here.  Our choices
                    // are:
                    //
                    // XML (load merge, load override, load replace)
                    // Raw configuration text (load merge, load override, load replace)
                    // 'set' config text (load set)
                    // 'diff' config (load diff)

                    // Simple test.  If first character is '<', then it is
                    // likely XML.

                    var trimmed = $(this).val().trim();

                    if (trimmed.charAt(0) == '<') {
                        type = 'XML';

                    } else if (trimmed.charAt(0) == '[') {
                        // If the first character is '[' then it is likely diff.
                        type = 'DIFF';
                    } else if (trimmed.substr(0, ('delete').length) == 'delete' ||
                        // If the first word is 'set' or 'delete', we are set config
                        trimmed.substr(0, ('set').length) == 'set') {
                        type = 'SET';
                    } else {
                        // Otherwise, raw text blurb
                        type = 'TEXT';
                    }

                    // Now, modify the input so that only available methods
                    // are selected.
                    var select = $(this).parent().find('div#buttonbar div select#loadStyle');

                    // Save our current mode so we don't mess with the DOM on
                    // every keypress when not necessary
                    if (select.attr('data-current') == type) {
                        return;
                    }
                    select.attr('data-current', type)

                    var selected = select.find('option:selected').attr('id');

                    select.children().removeAttr('disabled');

                    switch (type) {
                    case 'XML':
                    case 'TEXT':
                        if (selected == 'loadpatch' || selected == 'loadset') {
                            select.val('load merge');
                        }
                        select.find('option#loadpatch').attr('disabled', 'disabled');
                        select.find('option#loadset').attr('disabled', 'disabled');
                        break;
                    case 'DIFF':
                        if (selected != 'loadpatch') {
                            select.val('load patch');
                        }
                        select.find('option#loadmerge').attr('disabled', 'disabled');
                        select.find('option#loadoverride').attr('disabled', 'disabled');
                        select.find('option#loadreplace').attr('disabled', 'disabled');
                        select.find('option#loadupdate').attr('disabled', 'disabled');
                        select.find('option#loadset').attr('disabled', 'disabled');
                        break;
                    case 'SET':
                        if (selected != 'loadset') {
                            select.val('load set');
                        }
                        select.find('option#loadmerge').attr('disabled', 'disabled');
                        select.find('option#loadoverride').attr('disabled', 'disabled');
                        select.find('option#loadreplace').attr('disabled', 'disabled');
                        select.find('option#loadupdate').attr('disabled', 'disabled');
                        select.find('option#loadpatch').attr('disabled', 'disabled');
                        break;
                    }
                });
            }.bind(this));
        },
        willDestroyElement: function () {
            this.$().off('keyup');
        }
    });

    jQuery.clira.commandFile({
        name: 'configuration',
        templatesFile: '/clira/templates/configuration.hbs',
        prereqs: [
        ],
        commands: [{
            command: 'load configuration',
            help: 'Load a configuration onto a device',
            templateName: 'configuration-load',
            arguments: [{
                name: 'target',
                type: 'string',
                help: 'Remote taget to load configuration to',
                nokeyword: true,
                mandatory: true
            }],
            execute: loadConfiguration
        }]
    });

    function loadConfiguration (view, cmd, parse, poss) {
        if (!poss.data.target) {
            $.clira.makeAlert(view, 'You must include a target '
                + 'for the \'load configuration\' command');
            return;
        }

        var output = {
            target: poss.data.target
        };

        Configuration.Target = poss.data.target;
        Configuration.View = view;

        view.get('controller').set('output', output);
    }
});

