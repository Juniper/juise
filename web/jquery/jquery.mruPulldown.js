/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Implement a "most recently used" pulldown, containing a small history
 * of previously used values.  The pulldown maintains a list of recent
 * values, accepts new values, and allows selection of old values.
 *
 * This is initialized from an associative array containg:
 *
 * name: human friendly name of this set of groups.
 *
 * icon_pulldown: the icon controlling the appearance and dismissal
 * (cancellation) of the pulldown.
 *
 * focus_after: field that receives focus after a value has been
 * selected.
 *
 *

jQuery.mruPulldown({
    name: "command-history",
    top: $('#command-top'),
    entryClass: "command-history-entry",
});

 */

jQuery(function ($) {
    function MruPulldown (info) {
        $.dbgpr("pulldown: init:", info.name, info);
        $.extend(this, {
            depth: 40,
            version: "0.0.1",
            shown: false,
        });
        $.extend(this, info);
        var that = this;

        /*
         * If the caller didn't give an icon, look for one.  If we 
         * can't find one, then we have to bail.
         */
        if (!this.pulldownIcon) {
            this.pulldownIcon = $('.icon-pulldown', this.top);
            if (!this.pulldownIcon) {
                return null;
            }
        }

        if (!this.target)
            this.target = $('.input', this.top);

        if (!this.history)
            this.history = $('.history', this.top);

        if (!this.focusAfter)
            this.focusAfter = this.target;

        if (!this.contents)
            this.contents = $('.contents', this.history);

        if (!this.focusAfter)
            this.focusAfter = this.target;

        if (!this.clearIcon)
            this.clearIcon = $('.icon-clear', this.top);

        this.clearIcon.text("Clear").button({
            text: false,
            icons: { primary: 'ui-icon-trash' },
        }).attr("tabindex", "-1").click(function () {
            $.dbgpr("pulldown: clear:", that.name);
            that.select("");
            that.focus();
        });

        this.pulldownIcon.button({
            text: false,
            icons: { primary: 'ui-icon-triangle-1-s' },
        }).attr("tabindex", "-1").click(function (e) {
            that.pulldown(e);
        });

        if (this.multiple) {
            $("input", this.multiple).button({
                text: true,
            });

            this.multiple.submit(function (e) {
                if (e)
                    e.preventDefault();
                that.close();

                var value = "";

                $('input:checked', that.history).each(function () {
                    if (that.name)
                        value += " " + that.name;
                    $(that).removeAttr('checked');
                });

                value = value.substr(1);

                if (value != "") {
                    that.select(value);
                    that.focusAfter.focus();
                }
            });

        }
    }

    $.extend(MruPulldown.prototype, {
        pulldown: function (e, noCall) {
            e.preventDefault();
            $.dbgpr("pulldown: click:", this.name);
            this.shown = !this.shown;
            if (this.shown) {
                var top, left, width;

                top = this.target.offset().top
                    + this.target.outerHeight();
                left = this.target.offset().left;
                width = this.target.outerWidth();

                $.dbgpr("pulldown: click:", this.name, top, left);

                this.history.css({
                    top: top,
                    left: left,
                    width: width,
                    display: "block",
                });

            } else {
                /*
                 * The pulldown icon was clicked when the history
                 * div was shown, which we retreat as a "cancel".
                 * Drop the pulldown and give the input field focus.
                 */
                $.dbgpr("pulldown: click-to-cancel:", this.name);
                this.history.css({ display: "none" });
                this.target.focus();
            }

            if (!noCall && this.click)
                this.click(this);
        },
        markUsed: function (value) {
            var that = this;

            $.dbgpr("pulldown: mark-used:", this.name, value);
            /*
             * Mark a value in the history as "used", pushing it
             * to the top of the MRU list.
             */
            var $match, $elt;
            $('.' + this.entryClass, this.contents).each(function (i, elt) {
                $elt = $(elt);
                if ($elt.eq(0).text() == value)
                    $match = $elt;
            });

            /*
             * If the match was found, remove it in preparation
             * for re-adding it below.  If it doesn't exist, make it
             */
            if ($match) {
                $.dbgpr("pulldown: mark-used:", this.name, "recent");
                if (this.multiple)
                    $match = $match.parent();

                $match.remove();
                this.contents.prepend($match);

            } else {
                var evalue = value.replace(/&/g, "&amp;")
                           .replace(/</g, "&lt;")
                           .replace(/>/g, "&gt;")

                $.dbgpr("pulldown: mark-used:", this.name, "new");
                var content = '<div class="' + this.entryClass + '">'
	                    + evalue + '</div>';
                if (this.multiple) {
                    content = '<div class="' + this.entryClass + '-parent">'
                        + '<input type="checkbox" name="' + evalue + '"/>'
                        + content + '</div>';
                }

                $match = jQuery(content);
                this.contents.prepend($match);

                this.trim();
            }

            $match.click(function (e) {
                if (that.multiple) {
                    var $inp = $('input', $match)
                    $inp.attr('checked', 'checked');

                    if (e.target.localName == "input")
                        return;

                    value = "";
                    $('input:checked', that.history).each(function () {
                        if (that.name)
                            value += " " + that.name;
                        $(that).removeAttr('checked');
                    });
                    value = value.substr(1);
                }

                if (value != "")
                    that.select(value);
                that.close();
                that.focus();
            });

            $('.empty', this.history).css({ display: "none" });
            this.contents.css({ display: "block" });
        },
        maxDepth: function (newDepth) {
            if (newDepth) {
                this.depth = newDepth;
            } else {
                return this.depth;
            }
        },
        trim: function () {
            /* XXX */
        },
        value: function () {
            return this.target.get(0).value;
        },
        select: function (value) {
            this.target.get(0).value = value;
            return this;
        },
        focus: function (value) {
            $.dbgpr("pulldown: focus:", this.name);
            this.target.focus();
            return this;
        },
        close: function () {
            if (this.shown) {
                $.dbgpr("pulldown: close:", this.name);
                this.shown = false;
                this.history.css({ display: "none" });
            }
            return this;
        },
        record: function (value) {
            if (value == null)
                return;

            var cmdHistory = localStorage.getItem('cmdHistory');

            if (cmdHistory == null)
                cmdHistory = new Array();
            else
                cmdHistory = JSON.parse(cmdHistory);
            
            cmdHistory.push({'command': value, 'on': new Date().getTime()});
            localStorage.setItem('cmdHistory', JSON.stringify(cmdHistory));
        },
        show: function () {
            var cmdHistory = localStorage.getItem('cmdHistory');

            if (cmdHistory == null)
                return [];
            else
                return JSON.parse(cmdHistory);
        },
        clear: function () {
            localStorage.setItem('cmdHistory', '[]');
        },
    });

    $.mruPulldown = function (info) {
        var res = new MruPulldown(info);
        return res;
    }
});
