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
    $.mruPulldown = function (info) {
        var me = {
            depth: 40,
            version: "0.0.1",
        }
        $.extend(me, info);
        $.dbgpr("pulldown: init:", me.name, me);

        var shown = false;

        /*
         * If the caller didn't give an icon, look for one.  If we 
         * can't find one, then we have to bail.
         */
        if (!me.pulldownIcon) {
            me.pulldownIcon = $('.icon-pulldown', me.top);
            if (!me.pulldownIcon) {
                return null;
            }
        }

        if (!me.target)
            me.target = $('.input', me.top);

        if (!me.history)
            me.history = $('.history', me.top);

        if (!me.focusAfter)
            me.focusAfter = me.target;

        if (!me.contents)
            me.contents = $('.contents', me.history);

        if (!me.focusAfter)
            me.focusAfter = me.target;

        if (!me.clearIcon)
            me.clearIcon = $('.icon-clear', me.top);

        me.pulldown = function (e, noCall) {
            e.preventDefault();
            $.dbgpr("pulldown: click:", me.name);
            shown = !shown;
            if (shown) {
                var top, left, width;

                top = me.target.offset().top
                    + me.target.outerHeight();
                left = me.target.offset().left;
                width = me.target.outerWidth();

                $.dbgpr("pulldown: click:", me.name, top, left);

                me.history.css({
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
                $.dbgpr("pulldown: click-to-cancel:", me.name);
                me.history.css({ display: "none" });
                me.target.focus();
            }

            if (!noCall && me.click)
                me.click(me);
        }

        me.pulldownIcon.click(me.pulldown);

        me.clearIcon.text("Clear").button({
            text: false,
            icons: { primary: 'ui-icon-trash' },
        }).attr("tabindex", "-1").click(function () {
            $.dbgpr("pulldown: clear:", me.name);
            me.select("");
            me.focus();
        });

        me.pulldownIcon.button({
            text: false,
            icons: { primary: 'ui-icon-triangle-1-s' },
        }).attr("tabindex", "-1").click(function () {
            me.pulldown();
        });

        me.markUsed = function (value) {
            $.dbgpr("pulldown: mark-used:", me.name, value);
            /*
             * Mark a value in the history as "used", pushing it
             * to the top of the MRU list.
             */
            var $match, $elt;
            $('.' + me.entryClass, me.contents).each(function (i, elt) {
                $elt = $(elt);
                if ($elt.eq(0).text() == value)
                    $match = $elt;
            });

            /*
             * If the match was found, remove it in preparation
             * for re-adding it below.  If it doesn't exist, make it
             */
            if ($match) {
                $.dbgpr("pulldown: mark-used:", me.name, "recent");
                if (me.multiple)
                    $match = $match.parent();

                $match.remove();
                me.contents.prepend($match);

            } else {
                var evalue = value.replace(/&/g, "&amp;")
                           .replace(/</g, "&lt;")
                           .replace(/>/g, "&gt;")

                $.dbgpr("pulldown: mark-used:", me.name, "new");
                var content = '<div class="' + me.entryClass + '">'
	                    + evalue + '</div>';
                if (me.multiple) {
                    content = '<div class="' + me.entryClass + '-parent">'
                        + '<input type="checkbox" name="' + evalue + '"/>'
                        + content + '</div>';
                }

                $match = jQuery(content);
                me.contents.prepend($match);

                me.trim();
            }

            $match.click(function (e) {
                if (me.multiple) {
                    var $inp = $('input', $match)
                    $inp.attr('checked', 'checked');

                    if (e.target.localName == "input")
                        return;

                    value = "";
                    $('input:checked', me.history).each(function () {
                        if (this.name)
                            value += " " + this.name;
                        $(this).removeAttr('checked');
                    });
                    value = value.substr(1);
                }

                if (value != "")
                    me.select(value);
                me.close();
                me.focus();
            });

            $('.empty', me.history).css({ display: "none" });
            me.contents.css({ display: "block" });
        }

        if (me.multiple) {
            $("input", me.multiple).button({
                text: true,
            });

            me.multiple.submit(function (e) {
                if (e)
                    e.preventDefault();
                me.close();

                var value = "";

                $('input:checked', me.history).each(function () {
                    if (this.name)
                        value += " " + this.name;
                    $(this).removeAttr('checked');
                });

                value = value.substr(1);

                if (value != "") {
                    me.select(value);
                    me.focusAfter.focus();
                }
            });

        }

        me.value = function () {
            return me.target.get(0).value;
        }

        me.select = function (value) {
            me.target.get(0).value = value;
            return me;
        }

        me.focus = function (value) {
            $.dbgpr("pulldown: focus:", me.name);
            me.target.focus();
            return me;
        }

        me.close = function () {
            if (shown) {
                $.dbgpr("pulldown: close:", me.name);
                shown = false;
                me.history.css({ display: "none" });
            }
            return me;
        }

        me.maxDepth = function (newDepth) {
            if (newDepth) {
                me.depth = newDepth;
            } else {
                return me.depth;
            }
        }

        me.trim = function () {
            /* XXX */
        }

        return me;
    }

});
