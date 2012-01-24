/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2011, Juniper Network Inc, All rights reserved
 * See ../Copyright for more information.
 *
 * Based on Janko Jovanovic's article:
 *   http://www.jankoatwarpspeed.com/post/2009/09/28/webform-wizard-jquery.aspx
 */

jQuery(function ($) {
    var wizid = 1;

    // Using the constants checks makes Firefox angry and slow.  Not good.
    $.validator.setDefaults({
        onkeyup: false,
        onblur: false,
        onfocusout: false,
        onclick: false,
    });

    $.dform.subscribe({
        wizard : function (options, type) {
            $.dbgpr("wizard:", options.type, "+", this.name, "/", this.type);
            var x;
            for (x in options) {
                $.dbgpr(">>", x, ":", options[x]);
            }

            var steps = options.steps ? options.steps : "fieldset";
            var $this = $(this);
            $this.dformWizard($(steps, $this), options);
        },
        button : function (options, type) {
            $.dbgpr("dfw: button:", options.label);
            $(this).button(options);
        }
    });

    $.fn.dfwForm = function (dopts) {
        $.dbgpr("dform: rebuilding options");
        var fopts = dfwBuildForm(dopts);
        $.dbgpr("dform: built:", fopts);
        $(this).buildForm(fopts);
    }

    $.fn.dformWizard = function (steps, options) {
        var $top = $(this);
        var count = steps.size();


        if (options.submitButton)
            $(".submit", $top).hide();

        if (options.show_steps)
            $top.before("<ul class='ui-dform-steps'></ul>");

        steps.each(function (i) {
            var $this = $(this);
            $this.wrap("<div class='ui-dform-step " + step(i) + "'></div>");
            $this.append("<div class='"
                         + step(i, "", "-buttons")
                         + step(undefined, " ", "-buttons") + "'></div>");

            var name = $this.find("legend").html();
            if (options.show_steps) {
                var desc = "<li class='" + step(i, "", "-desc") + "'>";
                if (options.step_numbers)
                    desc += "<span class='ui-dform-step-number'>Step "
                    + (i + 1) + "</span>";
                if (name)
                    desc += "<span class='ui-dform-step-name'>"
                    		+ name + "</span>";
                desc += "</li>"

                $(".ui-dform-steps", $top).append(desc);
            }

            if (i == 0) {
                createCancelButton(i);
                createNextButton(i);

            } else if (i == count - 1) {
                createPrevButton(i);
                createCancelButton(i);

            } else {
                createPrevButton(i);
                createCancelButton(i);
                createNextButton(i);
            }

        });

        createSubmitButton();
        selectStep(0);

        function step (i, prefix, suffix) {
            prefix = (prefix === undefined) ? "" : prefix;
            suffix = (suffix === undefined) ? "" : suffix;
            i = (i === undefined) ? "" : "-" + i;

            return prefix + "ui-dform-step" + suffix + i;
        }

        function createPrevButton (i) {
            var tag = options.prevTag || "&lt; Back";

            $(step(i, ".", "-buttons"), $top)
	            .append("<a href='#' class='ui-dform-wizard-prev "
                            + step(i, "", "-prev") + "'>" + tag + "</a>");

            $(step(i, ".", "-prev"), $top).button().click(function (e) {
                e.preventDefault();
                if (options.validateBack && !validateForm())
                    return;

                var p = i - 1;

                if (options.prev)
                    p = options.prev(options, $top, i);

                selectStep(p);
            });
        }

        function createNextButton (i) {
            var tag = options.nextTag || "Next &gt;";

            $(step(i, ".", "-buttons"), $top)
	            .append("<a href='#' class='ui-dform-wizard-next "
                            + step(i, "", "-next") + "'>" + tag + "</a>");

            $(step(i, ".", "-next"), $top).button().click(function (e) {
                e.preventDefault();
                if (!validateForm())
                    return;

                var p = i + 1;

                if (options.next)
                    p = options.next(options, $top, i);

                selectStep(p);
            });
        }

        function createCancelButton (i) {
            if (!options.cancel)
                return;

            var tag = options.cancelTag || "Cancel";

            $(step(i, ".", "-buttons"), $top)
	            .append("<a href='#' class='ui-dform-wizard-cancel "
                            + step(i, "", "-cancel") + "'>" + tag + "</a>");

            $(step(i, ".", "-cancel"), $top).button().click(function (e) {
                e.preventDefault();
                options.cancel(options, $top);
            });
        }

        function selectStep(i) {
            if (i == count - 1)
                $(".submit", $top).show();
            else
                $(".submit", $top).hide();

            /* Hide all the steps except the current one */
            $(".ui-dform-step", $top).hide();
            $(step(i, "."), $top).show();

            if (options.show_steps) {
                $(step(i, ".", " li"), $top).removeClass("ui-dform-wizard-current");
                $(step(i, ".", "-desc"), $top).addClass("ui-dform-wizard-current");
            }
        }

        function createSubmitButton () {
            var sub = options.submit || {};
            var loc = sub.location;
            if (loc === undefined)
                loc = step(count - 1, ".", "-buttons");

            var $loc = $(loc, $top);
            $loc.each(function () {
                var $this = $(this);
                var value = options
                $this.append("<a href='#' class='ui-dform-submit'>"
                             + sub.value + "</a>");
                /* Bind the new "submit" button to the submit's click func */
                $(".ui-dform-submit", $this).button().click(function (e) {
                    e.preventDefault();
                    if (validateForm()) {
                        $.dbgpr("calling sub.click");
                        sub.click(options, $top);
                    }
                })
            });
        }

        function validateForm () {
            var valid = true;        /* "true" is all good news */
            if ($.fn.validate === undefined)
                return valid;

            var validator = $top.validate();

            // Look at the inputs for each non-hidden step in our form
            $("div.ui-dform-step:not(:hidden) input", $top).each(function () {
                var tvalid = validator.element(this);
                if (tvalid === undefined) // Agnostic?  Force a value!
                    tvalid = true;
                valid &= tvalid;
                $.dbgpr("validateForm:", this.name, ":", tvalid);
            })

            $.dbgpr("validateForm:", this.name, " is ", valid);
            return valid;
        }

        $.validator.addMethod("ui-dform-input", function(value, element) {
            var $element = $(element)
            function match (index) {
                return $(element).parents("#sf" + (index + 1)).length;
            }
            if (match(current))
                return !this.optional(element);

            return "dependency-mismatch";
        }, $.validator.messages.required);
    }

    var idgen = 1;
    function dfwBuildForm ($top, dopts) {


        var fopts = {};
        if (dopts.preferences) {
            fopts.dialog = {
                title: dopts.title,
                modal: true,
                draggable: false,
                position: 'top',
                resizable: false,
                show: 'blind',
                width: 600,
            }
            fopts.elements = [];

            var key, df = dopts.fields, out, dtype;
            for (key in df) {
                dtype = df[key].type;
                id = idgen++;
                sid = "ui-dfw-id-" + id;

                if (dtype == "boolean") {
                    // Put each field into its own div
                    out = {
                        type: "div",
                        class: "ui-dfw-input-div prefs-item",
                        elements: [
                            {
                                type: "label",
                                html: df[key].title,
                            },
                            {
                                name: key,
                                type: "checkbox",
                                id: sid,
                                class: "ui-dfw-input-field",
                                button: {
                                    label: "On",
                                },
                            },
                        ],
                    }

                } else {
                    // Put each field into its own div
                    out = {
                        type: "div",
                        class: "ui-dfw-input-div prefs-item",
                        elements : [
                            {
                                type: "label",
                                for: sid,
                                html: df[key].title,
                            },
                            {
                                name: key,
                                type: "text",
                                id: sid,
                                class: "ui-dfw-input-field",
                            },
                        ],
                    }
                }

                fopts.elements.push(out);
            }

        } else if (dopts.wizard) {
        } else {
        }

        return fopts;
    }
});
