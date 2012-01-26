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
    var idgen = 1;

    $.yform = function (guide, selector, fields) {
        // Allow alternate method of arguments
        if (selector === undefined)
            selector = guide.selector;
        if (fields === undefined)
            fields = guide.fields;
        if (fields === undefined)
            fields = [ ];

        // Find our wrapper.  If it doesn't exist, bail.
        var $wrapper = $(selector);
        if ($wrapper.length == 0)
            return undefined;

        var yform = { };
        yform.guide = guide;
        yform.id = idgen++;
        yform.selector = selector;
        yform.fields = fields;
        yform.wrapper = $wrapper;
        yform.data = { };

        var $form = $("<form id='ui-yf-form-" + yform.id
                      + "' action='#'></form>");
        $wrapper.append($form);
        yform.form = $form;

        if (guide.title)
            $wrapper.attr("title", guide.title);

        if (guide.preferences) {
            yform.dialog = {
                autoOpen: false,
                width: 600,
                modal: true,
                draggable: false,
                position: 'top',
                resizable: false,
                show: 'blind',

                open: function () {
                    //
                    // By default, there is no linkage between hitting
                    // enter and submitting a form.  In our HTML, we
                    // make a fake "submit" button so the browser does
                    // the "right" thing, but here we hijack it and
                    // make it use our own submit logic to click on
                    // the "Apply" button.
                    //
                    yform.showing = true;
                    $form.unbind('submit');
                    $form.submit(function (e) {
                        e.preventDefault();
                        $("button :last", $wrapper).click();
                        return false;
                    })
                    if (yform.dialog.afterOpen)
                        yform.dialog.afterOpen(yform);
                },
                buttons: {
                    "Restore defaults": function() {
                        yform.restore();
                        loadForm(yform.data);
                        yform.close();
                    },
                    Cancel: function() {
                        yform.close();
                    },
                    Apply: function() {
                        yform.apply();
                        yform.close();
                    },
                },
                close: function() {
                    yform.showing = false;
                    /* allFields.removeClass( "ui-state-error" ); */
                    if (yform.dialog.afterClose)
                        yform.dialog.afterClose(yform);
                }
            }
            $.extend(yform.dialog, guide.dialog);
        }

        yform.shown = function() {
            return yform.showing;
        }

        yform.restore = function (initial) {
            var data = yform.data;
            for (var f = 0; f < fields.length; f++) {
                var info = fields[f];
                var key = info.name;
                var prev = data[key];
                var value = info.def;
                if (info.type == "boolean")
                    value = (value === true || value == "true");
                prefsSetValue(info, key, value, info.type, initial);
            }

            return data;
        }

        yform.apply = function () {
            $.dbgpr("yform.apply");

            for (var f = 0; f < fields.length; f++) {
                var info = fields[f];
                var key = info.name;
                var value;

                if (info.type == "boolean") {
                    value = $("input[name=" + key + "]:checked", $form);
                    value = (value && value.length != 0);
                } else {
                    value = $("input[name=" + key + "]", $form).val();
                }

                prefsSetValue(info, key, value, info.type);
            }

            return false;
        }

        function prefsSetValue (info, name, value, type, initial) {
            if (!initial)
                $.dbgpr("prefsSetValue",  info, name, value, type);
            if (type == "boolean") {
                value = (value === true || value == "true");
                if (yform.data[name] == value)
                    return;
            } else if (type == "string") {
                if (yform.data[name] == value)
                    return;
            } else {
                var ival = parseInt(value);

                if (!isNaN(ival)) {
                    if (yform.data[name] == ival)
                        return;
                    value = ival;

                } else if (type == "number-plus") {
                    if (yform.data[name] == value)
                        return;
                } else {
                    return;     // Can't set numbers to garbage
                }
            }

            var prev = yform.data[name];
            if (!initial)
                $.dbgpr("prefs: ", name, "set to", value, "; was ", prev);
            yform.data[name] = value;
            if (info.change)
                info.change(value, initial, prev);
        }

        yform.buildForm = function () {
            if (this.built) 
                return;
            this.built = true;

            if (guide.tabs)
                yform.form.append("<ul class='ui-yf-tablist'></ul>");

            if (guide.preferences) {
                buildFormData();
                loadForm(yform.data);
                $wrapper.dialog(yform.dialog);

            } else if (guide.command) {
                buildFormData();
                loadForm(yform.data);

                $form
                    .append("<button class='ui-yf-execute'>Ok</button>"
                            + "<input type='submit' style='display: none'/>");
                $form.unbind('submit');
                $form.submit(function (e) {
                    e.preventDefault();
                    $.dbgpr("submit: redirecting...");
                    $(".ui-yf-execute", $form).click();
                    return false;
                })
                                  
                $(".ui-yf-execute", $form).button().click(function (event) {
                    event.preventDefault();
                    $.dbgpr("yform command execute button", $(this).text());
                    yform.guide.command.execute(yform);
                });

            } else {
                /* ... */
            }

            if (guide.tabs) {
                var $tablist = $(".ui-yf-tablist", yform.form);
                $("fieldset", yform.form).each(function (i, fs) {
                    $tablist.append("<li><a href='#" + $(fs).attr("id")
                                    + "'>" + $("legend", fs).text()
                                    + "</a></li>");
                    $("legend", fs).remove();
                });

                var tabs = $.extend({
                    // event: "mouseover",
                }, guide.tabs);
                yform.form.tabs(tabs);

                yform.form.submit(function (e) {
                    $.dbgpr("yform.submit");
                    e.preventDefault();
                    $("button :last", yform.form).click();
                    return false;
                });
            }
        }

        function loadForm (data) {
            for (var f = 0; f < fields.length; f++) {
                var info = fields[f];
                var key = info.name;
                if (info.type == "boolean") {
                    if (data[key] === true || data[key] == "true")
                        $("input[name=" + key + "]", $form).attr("checked", "checked");
                    else
                        $("input[name=" + key + "]", $form).removeAttr("checked");
                } else {
                    $("input[name=" + key + "]", $form).val(data[key]);
                }
            }
        }

        function unloadForm () {
            var data = yform.data;
            for (var f = 0; f < fields.length; f++) {
                var info = fields[f];
                var key = info.name;
                if (info.type == "boolean") {
                    var $cb = $("input[name=" + key + "]:checked", $form);
                    data[key] = ($cb && $cb.length != 0);
                } else {
                    data[key] = $("input[name=" + key + "]", $form).val(); 
                }
            }
        }

        function buildFormData () {
            /* Supports either simple fields, or fieldsets */
            var parent = $form;

            if (guide.fieldsets) {
                for (var fn = 0; fn < guide.fieldsets.length; fn++) {
                    var fs = guide.fieldsets[fn];
                    buildFieldSet(parent, fs.fields, true, idgen++, fs.legend);
                }
            } else {
                buildFieldSet(parent, fields, false, idgen++);
            }
        }

        function buildFieldSet (parent, fields, save_fields, id, legend) {
            var $fs = $("<fieldset id='" + id
                        + "' class='ui-yf-fieldset'></fieldset>");
            $fs.appendTo(parent);
            if (legend)
                $fs.append("<legend>" + legend + "</legend>");

            for (var f = 0; f < fields.length; f++) {
                var info = fields[f];
                var is_input = true;
                var key = info.name;

                if (info.type == "html") {
                    $fs.append(jQuery(info.html));
                    continue;
                }

                var content = "<div class='ui-yf-item'>";
                var lid = "ui-yf-" + id + "-" + key;

                if (info.type == "boolean") {
                    content += "<label>" + info.title + "</label>";
                    content += "<div class='ui-yf-boolean'>";
                    content += "<input name='" + key + "' id='" + lid
                        + "' type='checkbox'/>";
                    content += "<label style='display: none' for='" + lid + "'>"
                        + info.label + "</label>";
                    content += "</div>";
                } else {
                    content += "<label for='" + lid + "'>"
                        + info.title + "</label>";

                    content += "<input name='" + key + "' id='" + lid
                        + "' type='text'/>";
                }

                content += "</div>";

                var $new = jQuery(content);
                $fs.append($new);

                if (info.type == "boolean") {
                    $("input", $new).checkbox({ empty: "/images/empty.png"});
                } else {
                    /* ... */
                }

                if (is_input && save_fields)
                    yform.fields.push(info);
            }
        }

        yform.open = function () {
            if (guide.preferences) {
                yform.buildForm();
                loadForm(yform.data);
                $wrapper.dialog("open");
            }
        }

        yform.close = function () {
            if (guide.preferences) {
                $wrapper.dialog("close");
            }
        }

        yform.focus = function (selector) {
            if (selector === undefined)
                selector = "input[type='text']";
            var $in = $(selector, yform.form);
            if ($in.length > 0)
                $in.get(0).focus();
        }

        yform.getData = function () {
            return yform.data;
        }

        yform.buildRpc = function () {
            var rpc = "<" + yform.guide.command.rpc ">";

            

            rpc += "</" + yform.guide.command.rpc ">";
            return rpc;
        }

        /*
         * (We are still in the constructor)
         * Return the object we made as a handle for future work
         */
        yform.restore(true);
        return yform;
    }
});
