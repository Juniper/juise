/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */


/*
 * Ember views for the jQuery UI widgets.
 */

// Ember view for the jQuery UI Autocomplete widget
JQ.AutoCompleteView = Ember.TextField.extend(JQ.Widget, {
    uiType: 'autocomplete',
    uiOptions: ['disabled', 'autoFocus', 'delay', 'minLength', 'position',
                'source'],
    uiEvents: ['close']
});

// Ember view for the JQuery UI Button widget
JQ.ButtonView = Em.View.extend(JQ.Widget, {
    uiType: 'button',
    uiOptions: ['label', 'disabled', 'icons', 'text'],
    tagName: 'button'
});

// Emebr view for jQuery UI Dialog widget
JQ.Dialog = Em.View.extend(JQ.Widget, {
    uiType: 'dialog',
    uiOptions: ['autoOpen', 'buttons', 'height', 'modal', 'resizable',
                'title', 'width'],
    uiEvents: ['open', 'close']
});


/*
 * Views to handle dynamic forms. To create dynamic forms, we must extend this
 * class and create a childView from the extended class by passing JSON object
 * containing fields object. Fields object contains hash of fields which hold
 * title, name, value, boolean values corresponding to each field. If boolean
 * is set to true, we will render a checkbox instead of textfield. User can
 * also specify an optional boolean flag 'secret' which will make the text
 * inputbox be used to accept secret data. In the overridden class, user 
 * should specify hash of buttons along with their click functions. view and 
 * viewContext global variables will be available in the button action 
 * functions. When the user modifies any value, it will be captured in 
 * fieldValues as name:value and the same can be retrieved from 
 * viewContext.get('fieldValues')
 */
Clira.DynFormView = JQ.Dialog.extend({
    templateName: "dyn_form",
    view: null,
    viewContext: null,
    width: "auto",

    // Set view and view context as globals
    didInsertElement: function() {
        this._super.apply(this, arguments);
        view = this;
        viewContext = view.get('context');
    },

    /*
     * Set fields and fieldValues as properties on controller so we can use
     * them in template and to capture the modified values
     */
    willInsertElement: function() {
        this.get('controller').set('message', this.get('message'));
        this.get('controller').set('fields', this.get('fields'));
        this.get('controller').set('fieldValues', {});
    }
});

/*
 * View to handle text inputbox in dynamic forms
 */
Clira.DynTextField = Ember.TextField.extend({
    // We observe on value changes and update fieldValues
    valueChange: function() {
        var fieldId = this.get('fieldId');
        var values = this.get('values');
        if (values && fieldId) {
            values[fieldId] = this.get('value');
        }
    }.observes('value')
});

/*
 * View to handle input checkbox in dynamic forms
 */
Clira.DynCheckbox = Ember.Checkbox.extend({
    // We observe on value changes and update fieldValues
    valueChange: function() {
        var fieldId = this.get('fieldId');
        var values = this.get('values');
        if (values && fieldId) {
            values[fieldId] = this.get('checked');
        }
    }.observes('checked')
});


/*
 * Extendable Clira specific Ember views.
 */

/*
 * Ember view for jQuery UI autocomplete widget. Our 'source' for autocomplete
 * comes from the view's controller which reads the value in input field and
 * spits out possible completions. In this view, we take care of decorating
 * the autocomplete items by overriding _renderItem() of autocomplete widget
 * after the view is inserted to the DOM.
 */
Clira.AutoComplete = JQ.AutoCompleteView.extend({
    attributeBindings: ['autocomplete', 'autofocus', 'maxlength', 
                        'spellcheck'],
    autocomplete: 'off',
    autofocus: 'on',
    classNames: ['command-value', 'input', 'text-entry'],
    maxlength: '2048',
    spellcheck: 'false',

    // Called after the view is added to the DOM
    didInsertElement: function() {
        this._super.apply(this, arguments);

        // Set the source as controller's autoComplete function
        this.ui.source = this.get('targetObject').autoComplete;

        // Handle focus events
        this.$().focus(function() {
            $('#command-input-box').attr('class', 'input-box focus-on');        
        });

        // Decorate autocomplete list
        this.ui._renderItem = function renderItemOverride(ul, item) {
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
        };
    },

    // Fade out the input box border on focus out
    focusOut: function() {
        $('#command-input-box').attr('class', 'input-box focus-off');
    },

    // Execute the command when the user hits return
    insertNewline: function() {
        this.ui.close();
        this.get('targetObject').executeCommand();
    }
});


/*
 * Extending Ember's ContainerView, this view deals with the output from each
 * command run. This view houses two child views, one for the header and the
 * other to wrap output from command run. Header is inserted into the
 * container during the view initialization and uses 'output_header' template. 
 * For the child view that displays command output, we read template name from
 * view's controller (if defined in command definition) and use the template
 * to render the output.
 */
Clira.OutputContainerView = Ember.ContainerView.extend({
    templateName: "output_container",
    classNames: ['output-wrapper', 'ui-widget', 'ui-widget-content', 
                 'ui-corner-all'],

    // Insert output header child
    init: function() {
        var outputHeader = Ember.View.create({
            templateName: 'output_header'
        });
        this.set('childViews', [outputHeader]);
        this._super();
    },

    /*
     * Once the view is added to the DOM, we have access to our controller.
     * Get template name to render the output with from the controller. Use
     * 'output_content_layout' to wrap the rendered output in the output
     * container.
     */
    didInsertElement: function() {
        // Emit messages if there are any
        if (this.get('controller').messages) {
            var messagesView = Ember.View.create({
                context: this.get('controller').messages,
                layoutName: "output_content_layout",
                templateName: "clira_messages"
            });
            this.pushObject(messagesView);
        }

        var poss = this.get('controller').poss,
            parse = this.get('controller').parse,
            command = this.get('controller').command;

        // Proceed with output generation only for valid commands
        if (!this.get('controller').parseErrors) {
            var contentView = Ember.View.create({
                layoutName: "output_content_layout",
                templateName: this.get('controller').contentTemplate,

                didInsertElement: function() {
                    // Call execute function once we are in DOM
                    poss.command.execute.call(null, contentView, command, 
                                                parse, poss);
                }
            });
            this.pushObject(contentView);
        }
    }
});


/*
 * Extend ButtonView to define a pulldown icon/button
 */
Clira.PulldownIcon = JQ.ButtonView.extend({
    label: null,
    text: false,
    icons: {
        primary: 'ui-icon-triangle-1-s'
    },

    click: function() {
        this.get('controller').toggleMruPulldown();
    }
});


/*
 * Extend ButtonView to defined Enter button with label
 */
Clira.EnterButton = JQ.ButtonView.extend({
    label: 'Enter',

    click: function() {
        this.get('parentView').CommandInput.insertNewline();
    }
})


/*
 * View to handle most recently used commands list pulldown. This view is 
 * displayed when user clicks on pulldown icon.
 */
Clira.MruPulldownView = Ember.View.extend({
    classNames: ['ui-menu', 'pulldown'],

    // Hide the view when not in focus
    mouseLeave: function() {
        this.set('isVisible', false);
    },

    // Observe controller's visible property and change visibility
    toggleVisibility: function() {
        this.toggleProperty('isVisible');
    }.observes('controller.visible')
});


/*
 * View to display commands in mru pulldown list
 */
Clira.MruItemView = Ember.View.extend({
    classNames: ['mru-item'],

    click: function() {
        var commandInputView = this.get('parentView').get('parentView')
                                   .CommandInput;
        // Set the command
        commandInputView.get('targetObject').set('command',this.content);
        commandInputView.$().focus();

        // Hide mru pulldown view
        this.get('parentView').get('controller').toggleProperty('visible');
    }
});


/*
 * View to display message along with an icon. Content passed to this view
 * should contain a 'message', 'type' and optional 'noIcon' boolean. Types 
 * 'error' and 'highlight' comes default with jquery-ui. When other types 
 * are used, make sure we define corresponding ui-state-<type> class in 
 * stylesheet
 */
Clira.MessageView = Ember.View.extend({
    templateName: "clira_message",
    uiType: function() {
        return "ui-state-" + this.get('content').type;
    }.property('content.type')
});


/*
 * View for clira preferences button
 */
Clira.PrefsButtonView = Ember.View.extend({
    classNames: ['prefsbtn'],

    // Create a preferences dialog as childView and append
    click: function() {
        this.createChildView(Clira.PreferencesDialog).append();
    }
});


/*
 * View to display preferences dialog and handle devices, group and general
 * preferences
 */
Clira.PreferencesDialog = Ember.View.extend({
    templateName: "preferences",
    isVisible: false,

    actions: {
        generalPref: function() {
            var prefs = Clira.Preference.find(),
                fields = [];

            // Sanitize before creating a form
            if (prefs) {
                prefs.forEach(function(item) {
                    pref = item.__data;
                    if (pref.type == "boolean") {
                        pref['boolean'] = true;

                        if (pref.value == "true") {
                            pref['value'] = true;
                        } else {
                            pref['value'] = false;
                        }
                    } else {
                        pref['boolean'] = false;
                    }
                    fields.push(pref);
                });
            }
            this.createChildView(Clira.GeneralPrefView, {fields: fields}).append();
        }
    },

    /*
     * We use jqGrid to read device and group config from db and display 
     * them in corresponding preferences modal
     */
    didInsertElement: function() {
        $.proxy($("#prefs-main-form").dialog({
            buttons: {
                'close': function() {
                    $(this).dialog("close");
                }
            },
            height: 210,
            resizable: false,
            width: 320
        }), this);

        // Build devices and group preferences form dialogs using jqGrid
        $.clira.buildPrefForms();
    }
});


/*
 * View extending dynamic forms to handle displaying and saving clira
 * preferences using localStorage backed ember-restless
 */
Clira.GeneralPrefView = Clira.DynFormView.extend({
    title: "Preferences",
    buttons: {
        cancel: function() {
            $(this).dialog('close');
        },
        save: function() {
            var clira_prefs = $.clira.prefs;
            // Iterate fieldValues and save them back into Clira.Preference
            $.each(viewContext.get('fieldValues'), function(k, v) {
                var pref = Clira.Preference.find(k);
                if (pref) {
                    pref.set('value', v);
                    pref.saveRecord();

                    // Update $.clira.pref
                    clira_prefs[k] = v;
                }
            });
            $(this).dialog('close');
        }
    }
});
