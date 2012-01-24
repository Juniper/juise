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
                "caption" : "Size information",
                "elements" : [
                    {
                        "name" : "size",
                        "caption" : "Dress Size",
                        "type" : "text",
                    },
                    {
                        "name" : "shoe",
                        "caption" : "Shoe size",
                        "type" : "text",
                        "validate" : { "required" : true }
                    },
                ],
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
                    },
                ]
            },
        ],
        wizard: {
            steps: "fieldset",
            next: function (options, $top, old) {
                var value = $("input[name='sex']:checked", $top).val();
                $.dbgpr("next", value);
                if (old == 0)
                    return (value == "m") ? 2 : 1;
                return old + 1;
            },
            prev: function (options, $top, old) {
                var value = $("input[name='sex']:checked", $top).val();
                $.dbgpr("prev", value);
                if (old == 2)
                    return (value == "m") ? 0 : 1;
                return 0;
            },
            submit: {
                location: ".ui-dform-step-buttons-2",
                value: "Apply",
                click: function (options, $top) {
                    $.dbgpr("click", this, ":", this.name);
                },
            },
            cancel : function (options, $top) {
                $.dbgpr("cancel");
            },
        },
    };

    $('#demo').buildForm(options);
});
