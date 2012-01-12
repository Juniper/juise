/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2011-2012, Juniper Network Inc, All rights reserved
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    var count = 0;

    $.dform.removeSubscription("elements");
    $.dform.subscribe({
        elements : function (options, type) {
            var $scoper = $(this);
            $.each(options, function(index, nested) {
                var values = nested;
                if (typeof (index) == "string")
                    values["name"] = name;
                $scoper.append("<div class='ui-dform-elements'></div>");
                var $new = $($scoper.children().get(-1));
                $new.formElement(values);
            });
        },
        wizard : function (options, type) {
            $.dbgpr("wizard:", options.type, "+", this.name, "/", this.type);
            var x;
            for (x in options) {
                $.dbgpr(">>", x, ":", options[x]);
            }
        },
    });

    var options = {
        "action" : "index.html",
        "method" : "post",
        "elements" : [
            {
                "type" : "fieldset",
                "caption" : "User information",
                "elements" : [
                    {
                        "name" : "email",
                        "caption" : "Email address",
                        "type" : "text",
                        "placeholder" : "E.g. user@example.com",
                        "validate" : {
                            "email" : true
                        }
                    },
                    {
                        "name" : "password",
                        "caption" : "Password",
                        "type" : "password",
                        "id" : "registration-password",
                        "validate" : {
                            "required" : true,
                            "minlength" : 5,
                            "messages" : {
                                "required" : "Please enter a password",
                                "minlength" : "At least {0} characters long"
                            }
                        }
                    },
                    {
                        "name" : "password-repeat",
                        "caption" : "Repeat password",
                        "type" : "password",
                        "validate" : {
                            "equalTo" : "#registration-password",
                            "messages" : {
                                "equalTo" : "Please repeat your password"
                            }
                        }
                    },
                    {
                        "type" : "radiobuttons",
                        "caption" : "Sex",
                        "name" : "sex",
                        "class" : "labellist",
                        "options" : {
                            "f" : "Female",
                            "m" : "Male"
                        }
                    },
                    {
                        "type" : "checkboxes",
                        "name" : "test",
                        "caption" : "Receive newsletter about",
                        "class" : "labellist",
                        "options" : {
                            "updates" : "Product updates",
                            "errors" : {
                                "value" : "security",
                                "caption" : "Security warnings",
                                "checked" : "checked"
                            }
                        }
                    }
                ]
            },
            {
                "type" : "fieldset",
                "caption" : "Address information",
                "elements" : [
                    {
                        "name" : "name",
                        "caption" : "Your name",
                        "type" : "text",
                        "placeholder" : "E.g. John Doe"
                    },
                    {
                        "name" : "address",
                        "caption" : "Address",
                        "type" : "text",
                        "validate" : { "required" : true }
                    },
                    {
                        "name" : "zip",
                        "caption" : "ZIP code",
                        "type" : "text",
                        "size" : 5,
                        "validate" : { "required" : true }
                    },
                    {
                        "name" : "city",
                        "caption" : "City",
                        "type" : "text",
                        "validate" : { "required" : true }
                    },
                    {
                        "type" : "select",
                        "name" : "continent",
                        "caption" : "Choose a continent",
                        "options" : {
                            "america" : "America",
                            "europe" : {
                                "selected" : "true",
                                "id" : "europe-option",
                                "value" : "europe",
                                "html" : "Europe"
                            },
                            "asia" : "Asia",
                            "africa" : "Africa",
                            "australia" : "Australia"
                        }
                    }
                ]
            },
            {
                "type" : "submit",
                "value" : "Signup"
            },
        ],
        wizard: {
            next: function () {
                $.dbgpr("next");
            },
        },
    };

    $('#demo').buildForm(options);

});
