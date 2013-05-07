/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2011-2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    var idgen = 1;

    var css_files = { };
    var js_files = { };

    $.yform = function (guide, selector, fields) {
        var yf = new YForm (guide, selector, fields);
        yf.create();
        yf.restore(true);
        return yf;
    }

    function YForm (guide, selector, fields) {
        // Allow alternate method of arguments
        if (selector === undefined)
            selector = guide.selector;
        if (fields === undefined)
            fields = guide.fields;
        if (fields === undefined)
            fields = [ ];

        $.extend(this, {
            guide: guide,
            id: idgen++,
            selector: selector,
            fields: fields,
            wrapper: $(selector),
            data: { },
        });
    }

    $.extend(YForm.prototype, {
        create: function create () {
            var that = this;
            this.form = $("<form id='ui-yf-form-" + this.id
                          + "' action='#'></form>");
            this.wrapper.append(this.form);

            if (this.guide.title)
                this.wrapper.attr("title", this.guide.title);

            if (this.guide.css) {
                this.addFile(css_files, this.guide.css, function (file) {
                    var $link = $("<link rel='stylesheet' type='text/css' "
                        + "href='" + file + "'/>");

                    $link.appendTo($("html > head"));
                });
            }

            if (this.guide.preferences) {
                this.dialog = {
                    autoOpen: false,
                    width: 600,
                    modal: true,
                    draggable: false,
                    position: "top",
                    resizable: false,
                    show: "blind",

                    open: function () {
                        //
                        // By default, there is no linkage between hitting
                        // enter and submitting a form.  In our HTML, we
                        // make a fake "submit" button so the browser does
                        // the "right" thing, but here we hijack it and
                        // make it use our own submit logic to click on
                        // the "Apply" button.
                        //
                        that.showing = true;
                        that.form.unbind("submit");
                        that.form.submit(function (e) {
                            e.preventDefault();
                            $("button :last", that.wrapper).click();
                            return false;
                        })
                        if (that.dialog.afterOpen)
                            that.dialog.afterOpen(that);
                    },
                    buttons: {
                        "Restore defaults": function() {
                            that.restore();
                            that.loadForm();
                            that.close();
                        },
                        Cancel: function() {
                            that.close();
                        },
                        Apply: function() {
                            that.apply();
                            that.close();
                        },
                    },
                    close: function() {
                        that.showing = false;
                        /* allFields.removeClass( "ui-state-error" ); */
                        if (that.dialog.afterClose)
                            that.dialog.afterClose(that);
                    }
                }
                $.extend(this.dialog, this.guide.dialog);
            }
        },
        shown: function() {
            return this.showing;
        },

        restore: function (initial) {
            var data = this.data;
            for (var f = 0; f < this.fields.length; f++) {
                var info = this.fields[f];
                var key = info.name;
                var prev = data[key];
                var value = info.def;
                if (info.type == "boolean")
                    value = (value === true || value == "true");
                this.prefsSetValue(info, key, value, info.type, initial);
            }

            return data;
        },

        apply: function () {
            $.dbgpr("yform: apply");

            for (var f = 0; f < this.fields.length; f++) {
                var info = this.fields[f];
                var key = info.name;
                var value;

                if (info.type == "boolean") {
                    value = $("input[name=" + key + "]:checked", this.form);
                    value = (value && value.length != 0);
                } else {
                    value = $("input[name=" + key + "]", this.form).val();
                }

                this.prefsSetValue(info, key, value, info.type);
            }

            return false;
        },

        buildForm: function (execute) {
            var that = this;
            if (this.built) 
                return;
            this.built = true;

            if (this.guide.tabs)
                this.form.append("<ul class='ui-yf-tablist'></ul>");

            this.buildFormData();
            this.loadForm();

            if (this.guide.preferences) {
                this.wrapper.dialog(this.dialog);

            } else if (this.guide.command) {
                this.form.append("<button class='ui-yf-execute'>Ok</button>");

                $(".ui-yf-execute", this.form).button().click(function (event) {
                    event.preventDefault();
                    $.dbgpr("yform command execute button", $(this).text());
                    execute(this, that.guide);
                });

            } else {
                /* ... */
            }

            if (this.guide.tabs) {
                var $tablist = $(".ui-yf-tablist", this.form);
                $("fieldset", this.form).each(function (i, fs) {
                    $tablist.append("<li><a href='#" + $(fs).attr("id")
                                    + "'>" + $("legend", fs).text()
                                    + "</a></li>");
                    $("legend", fs).remove();
                });

                var tabs = $.extend({
                    // event: "mouseover",
                }, this.guide.tabs);
                this.form.tabs(tabs);

                this.form.submit(function (e) {
                    $.dbgpr("yform: submit");
                    e.preventDefault();
                    $("button :last", that.form).click();
                    return false;
                });
            }
        },

        open: function () {
            if (this.guide.preferences) {
                this.buildForm();
                this.loadForm();
                this.wrapper.dialog("open");
            }
        },

        close: function () {
            if (this.guide.preferences) {
                this.wrapper.dialog("close");
            }
        },

        focus: function (selector) {
            if (selector === undefined)
                selector = "input[type='text']";
            var $in = $(selector, this.form);
            if ($in.length > 0)
                $in.get(0).focus();
        },

        getData: function () {
            return this.data;
        },

        buildRpc: function () {
            var rpc = "<" + this.guide.command.rpc + ">";

            for (var f = 0; f < this.fields.length; f++) {
                var info = this.fields[f];
                var key = info.name;
                var value;

                if (info.type == "boolean") {
                    value = $("input[name=" + key + "]:checked", this.form);
                    value = (value && value.length != 0);
                    if (value)
                        rpc += "<" + key + "/>";
                } else {
                    value = $("input[name=" + key + "]", this.form).val();
                    if (value != "")
                        rpc += "<" + key + ">" + value + "</" + key + ">";
                }
            }

            rpc += "</" + this.guide.command.rpc + ">";
            return rpc;
        },

        prefsSetValue: function prefsSetValue (info, name, value, type,
                                               initial) {
            if (!initial)
                $.dbgpr("yform: prefsSetValue ", info, name, value, type);
            if (type == "boolean") {
                value = (value === true || value == "true");
                if (this.data[name] == value)
                    return;
            } else if (type == "string") {
                if (this.data[name] == value)
                    return;
            } else {
                var ival = parseInt(value);
                
                if (!isNaN(ival)) {
                    if (this.data[name] == ival)
                        return;
                    value = ival;
                
                } else if (type == "number-plus") {
                    if (this.data[name] == value)
                        return;
                } else {
                    return;     // Can't set numbers to garbage
                }
            }

            var prev = this.data[name];
            if (!initial)
                $.dbgpr("prefs: ", name, "set to", value, "; was ", prev);
            this.data[name] = value;
            if (info.change)
                info.change(value, initial, prev);
        },

        loadForm: function loadForm () {
            var data = this.data;
            for (var f = 0; f < this.fields.length; f++) {
                var info = this.fields[f];
                var key = info.name;
                if (info.type == "boolean") {
                    if (data[key] === true || data[key] == "true")
                        $("input[name=" + key + "]", this.form)
                            .attr("checked", "checked");
                    else
                        $("input[name=" + key + "]", this.form)
                            .removeAttr("checked");
                } else {
                    $("input[name=" + key + "]", this.form).val(data[key]);
                }
            }
        },

        unloadForm: function unloadForm () {
            var data = this.data;
            for (var f = 0; f < this.fields.length; f++) {
                var info = this.fields[f];
                var key = info.name;
                if (info.type == "boolean") {
                    var $cb = $("input[name=" + key + "]:checked", this.form);
                    data[key] = ($cb && $cb.length != 0);
                } else {
                    data[key] = $("input[name=" + key + "]", this.form).val(); 
                }
            }
        },

        buildFormData: function buildFormData () {
            /* Supports either simple fields, or fieldsets */
            var parent = this.form;

            if (this.guide.fieldsets) {
                for (var fn = 0; fn < this.guide.fieldsets.length; fn++) {
                    var fs = this.guide.fieldsets[fn];
                    this.buildFieldSet(parent, fs.fields, true, idgen++,
                                       fs.legend);
                }
            } else {
                this.buildFieldSet(parent, this.fields, false, idgen++);
            }
        },

        buildFieldSet: function buildFieldSet (parent, fields, save_fields,
                                               id, legend) {
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
                    this.fields.push(info);
            }
        },

        addFile: function addFile (list, name, func) {
            if (list[name] === undefined) {
                list[name] = 1;
                func(name);
            } else {
                list[name] += 1;
            }
        },
    });
});
