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
        this.ui.source = this.get('controller').autoComplete;

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
        this.get('controller').executeCommand();
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
                templateName: "clira_messages"
            });
            this.pushObject(messagesView);
        }

        // Insert output child if output is not null
        if (this.get('controller').output) {
            var contentView = Ember.View.create({
                context: this.get('controller').output,
                layoutName: 'output_content_layout',
                templateName: this.get('controller').contentTemplate
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
        commandInputView.get('controller').set('command',this.content);
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
