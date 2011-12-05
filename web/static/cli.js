/*
 * $Id$
 *  -*-  indent-tabs-mode:nil;  -*-
 * Copyright 2011, Juniper Network Inc, All rights reserved
 * See ../Copyright for more information.
 */

jQuery(function ($) {
    decorateIcons($('#debug-container'));
    $.dbgpr("document is ready");

    var $output = $('#output-top');
    var command_number = 0;
    var cmdHistory, tgtHistory;

    function cliInit () {
        cmdHistory = $.mruPulldown({
            name: "command-history",
            top: $('#command-top'),
            clearIcon: $('#command-clear'),
            entryClass: "command-history-entry",
            click: function (me) {
                if (tgtHistory)
                    tgtHistory.close();
            },
        });

        tgtHistory = $.mruPulldown({
            name: "target-history",
            top: $('#target-top'),
            clearIcon: $('#target-clear'),
            multiple: $('#target-history-form'),
            history: $('#target-history'),
            entryClass: "target-history-entry",
            focusAfter: $('#command-input'),
            click: function (me) {
                if (cmdHistory)
                    cmdHistory.close();
            },
        });

        $('#target-input-form').submit(commandSubmit);
        $('#command-input-form').submit(commandSubmit);

        $('#input-enter').text("Enter").button({
            text: true,
        }).click(function (e) {
            commandSubmit(e);
            cmdHistory.focus();
            $(this).blur();
            return false;
        });
    }

    function enterSubmit(e) {
    }

    function commandSubmit (event) {
        event.preventDefault();
        $.dbgpr("commandSubmit:", event.type);

        tgtHistory.close();
        cmdHistory.close();

        var command = cmdHistory.value();
        if (command == "") {
            $.dbgpr('submit; skipping since command value is empty');
            cmdHistory.focus();
            return false;
        }

        /*
         * Since multiple targets are allowed in the target field,
         * we need to break the target string up into distinct values.
         */
        var count = 0;
        var tset = [ ];
        var tname = "";
        var cname = "";
        var value = tgtHistory.value();
        $(value.split(" ")).each(function (i, x) {
            if (x != "") {
                count += 1;
                tset.push(x);
                commmandWrapperAdd(x, command);
                tgtHistory.markUsed(x);
                tname += " " + x;
                cname += "_" + x.replace("_", "__", "g");
            }
        });

        tname = tname.substr(1);
        cname = cname.substr(1);
        if (cname && cname != "")
            targetListMarkUsed(this, tname, cname);

        cmdHistory.markUsed(command);
        commandOutputTrim(count);

        return false;
    }

    function targetListMarkUsed (form, target, cname) {
        var $tset = $('#target-contents');
        var id = 'target-is-' + cname;
        var $target = $('#' + id, $tset);

        if ($target && $target.length > 0) {
            $target.remove();
            $tset.prepend($target);

        } else {

            var content = '<div id="' + id + '" class="target ' + id 
                + ' target-info rounded buttonish green">'
                + target
                + '</div>';

            var $target = jQuery(content);
            $tset.prepend($target);
            targetListTrim($tset);
        }

        $target.click(function (event) {
            tgtHistory.close();
            tgtHistory.select(target);
            cmdHistory.focus();
        });

        $('#target-contents-none').css({ display: "none" });
    }

    function targetListTrim ($tset) {
        $('.target', $tset).each(function (index, target) {
            var delta = $(target).position().top -
                        $(target).parent().position().top;
            if ( delta > prefs.max_target_position) {
                $(target).remove();
            }
        });     
    }

    function commmandWrapperAdd (target, command) {
        $.dbgpr("commmandWrapperAdd", target, command);

        var content = '<div class="output-wrapper ui-widget ui-corner-all">'
            + '<div class="output-header ui-widget-header ui-corner-all">'
            + '<button class="icon-remove-section"></button>'
            + '<button class="icon-hide-section hidden"></button>'
            + '<button class="icon-unhide-section"></button>'
            + '<button class="keeper icon-keeper-section"></button>';

        if (target) {
            content += '<div class="target-value rounded buttonish green">'
                + target
                + '</div><div class="label"> -> </div>';
        }

        content += '<div class="command-value rounded blue">'
                + command
                + '</div><div class="command-number">'
                + " (" + ++command_number + ")"
                + '</div></div><div class="output-content can-hide" '
                +    'style="white-space: pre-wrap">';

        if (prefs.live_action) {
            content += '<img src="/icons/loading.png">';
            content += "    ....loading...\n";
        } else {
            content += "test-output\nmore\nand more and more\noutput\n";
        }

        content += '</div>';

        var $newp = jQuery(content);
        $output.prepend($newp);
        $newp.slideUp(0).slideDown(prefs.slide_speed);
        divUnhide($newp);

        if (prefs.live_action) {
            var $out = $(".output-content", $newp);
            $out.slideUp(0).slideDown(prefs.slide_speed);

            $out.load("/bin/cmd.slax",
                      { target: target, command: command, form: "false" },
                      function (text, status, http) {
                          $.dbgpr("target", target);
                          $.dbgpr("command", command);
                          $.dbgpr("text", text);
                          $.dbgpr("status", status);
                          $.dbgpr("http", http);
                          if (status == "error") {
                              $(this).html('<div class="command-error">'
                                           + 'An error occurred: '
                                           + http.statusText
                                           + '</div>');
                          } else {
                              var $xml = $($.parseXML(text));
                              var $err = $xml.find("[nodeName='xnm:error']");
                              if ($err[0])
                                  $(this).html('<div class="command-error">'
                                     + 'An error occurred: '
                                     + htmlEscape($("message", $err).text())
                                     + '</div>');
                          }
                          $out.slideDown(prefs.slide_speed);
                      });
        }

        decorateIcons($newp);

        $('.target-value', $newp).click(function () {
            tgtHistory.select($(this).text());
        });

        $('.command-value', $newp).click(function () {
            cmdHistory.select($(this).text());
        });

        return false;
    }

    function htmlEscape (val) {
        return val.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;");
    }

    function commandOutputTrimChanged () {
        commandOutputTrim(0);
    }

    function commandOutputTrim (fresh_count) {
        var last = $output.get(0);
        if (!last)
            return;

        var keep = 0;
        for (var i = 0; last.children[i]; i++) {
            var $child = $(last.children[i]);
            if ($child.hasClass("keeper-active")) {
                keep += 1;
            } else if (i < fresh_count) {
                /* do nothing */
            } else if (i >= prefs.output_remove_after + keep) {
                divRemove($child);
            } else if (i >= prefs.output_close_after + keep) {
                if (!divIsHidden($child))
                    divHide($child);
            } else {
                if (divIsHidden($child))
                    divUnhide($child);
            }
        }
    }

    function divRemove ($wrapper) {
        $wrapper.slideUp(prefs.slide_speed, function () {
            $wrapper.remove();
        });
    }

    function divIsHidden ($wrapper) {
        return $('.icon-hide-section', $wrapper).hasClass("hidden");
    }

    function divHide ($wrapper) {
        $('.icon-unhide-section', $wrapper).removeClass("hidden");
        $('.icon-hide-section', $wrapper).addClass("hidden");
        $('.can-hide', $wrapper).slideUp(prefs.slide_speed);
    }

    function divUnhide ($wrapper) {
        $('.icon-unhide-section', $wrapper).addClass("hidden");
        $('.icon-hide-section', $wrapper).removeClass("hidden");
        $('.can-hide', $wrapper).slideDown(prefs.slide_speed);
    }

    /*----------------------------------------------------------------------
     * Preferences allow customizable behavior.  We have our preferences
     * described in the array "prefsInfo".  We keep the current values in
     * the "prefs" array.
     *
     * XXX need to make radio buttons, pulldowns, etc for enum types.
     * Need to add ranges.  Need verifiation code.  Probably should
     * use the forms plugin.
     */

    var prefs_info = {
        output_close_after: { def: 5, type: "number",
                              change: commandOutputTrimChanged,
                              title: "Number of open commands" },
        output_remove_after: { def: 10, type: "number",
                               change: commandOutputTrimChanged,
                               title: "Total Number of commands" },
        slide_speed: { def: "fast", type: "number-plus",
                       title: "Speed of slide animations" },
        max_commands_list: { def: 40, type: "number",
                            title: "Number of commands kept in list" },
        max_targets_inline: { def: 10, type: "number",
                              title: "Number of targets kept on screen" },
        max_targets_list: { def: 40, type: "number",
                            title: "Number of targets kept in list" },
        max_target_position: { def: 10, type: "number",
                               title: "Target button rows" },
        stay_on_page: { def: false, type: "boolean",
                        change: prefsSetupConfirmExit,
                        title: "Give warning if leaving this page" },
        theme: { def: "black-tie", type: "string",
                 title: "UI Theme", change: prefsSwitchTheme },
        live_action: { def: false, type: "boolean",
                        title: "Interact with real devices" },
    };

    var prefs = { };

    var prefs_shown = false;
    var prefs_form_built = false;

    function prefsInit () {
        for (var key in prefs_info) {
            prefs[key] = prefs_info[key].def;
            if (prefs_info[key].change)
                prefs_info[key].change(prefs_info[key].def, true);
        }

        $('#prefs').button().click(prefsEdit);
    }

    function prefsEdit (event) {
        event.preventDefault();

        var $this = $(this);
        var $pform = $('#prefs-form');

        prefs_shown = !prefs_shown;
        if (prefs_shown) {
            var form = $pform.get(0);

            if (!prefs_form_built) {
                prefs_form_built = true;
                for (var key in prefs_info) {
                    var info = prefs_info[key];
                    var content = '<div class="prefs-item">'
                        + '<label for="' + key + '">' + info.title
                        + '</label>'
                        + '<input name="' + key + '" type="text"/>'
                        + '<br/></div>';
                    var $target = jQuery(content);
                    $pform.prepend($target);
                }
            }

            for (var key in prefs_info) {
                form[key].value = prefs[key];
            }

            $pform.parent().css({
                display: "block",
                top: $this.offset().top + 30,
                left: $this.offset().left - 400,
                width: "400px",
                "background-color": "#ffffff",
            });

            $pform.submit(function (event) {
                event.preventDefault();
                prefs_shown = false;
                $pform.parent().css({ display: "none" });

                for (var key in prefs_info) {
                    var info = prefs_info[key];
                    prefsSetValue(info, key, this[key].value, info.type);
                }
            });

            $('#prefs-restore').click(function (event) {
                event.preventDefault();

                $.dbgpr("prefs-restore");
                for (var key in prefs_info) {
                    prefs[key] = prefs_info[key].def;
                    if (prefs_info[key].change)
                        prefs_info[key].change(prefs_info[key].def, false);
                    form[key].value = prefs_info[key].def;
                }
            });

        } else {
            $pform.parent().css({ display: "none" });
        }
    }

    function prefsSetValue (info, name, value, type) {
        if (type == "boolean") {
            value = (value == "true");
            if (prefs[name] == value)
                return;
        } else {
            var ival = parseInt(value);

            if (!isNaN(ival)) {
                if (prefs[name] == ival)
                    return;
                value = ival;

            } else if (type == "number-plus") {
                if (prefs[name] == value)
                    return;
            }
        }

        var prev = prefs[name];
        $.dbgpr("prefs: ", name, "set to", value, "; was ", prev);
        prefs[name] = value;
        if (info.change)
            info.change(value, true, prev);
    }

    function prefsSetupConfirmExit () {
        if (prefs.stay_on_page) {
            window.onbeforeunload = function (e) {
                return "Are you sure?  You don't look sure.....";
            }
        } else {
            window.onbeforeunload = null;
        }
    }

    function prefsSwitchTheme (value, initial, prev) {
        if (initial)
            return;

        $("link.ui-theme, link.ui-addon").each(function (i, $this) {
            var attr = $this.attr("href");
            attr.replace(prev, value);
            $this.attr("href", attr);
        });
    }

    /*
     * Our dbgpr container decorations (which need some of our functions)
     */
    function decorateIcons ($wrapper) {
        $('.icon-remove-section', $wrapper).text("Close").button({
            text: false,
            icons: { primary: 'ui-icon-closethick' },
        }).click(function () {
            divRemove($wrapper);
        });

        $('.icon-hide-section', $wrapper).text("Hide").button({
            text: false,
            icons: { primary: 'ui-icon-minusthick' },
        }).click(function () {
            divHide($wrapper);
        });

        $('.icon-unhide-section', $wrapper).text("Unhide").button({
            text: false,
            icons: { primary: 'ui-icon-plusthick' },
        }).click(function () {
            divUnhide($wrapper);
        }).addClass("hidden");

        $('.icon-clear-section', $wrapper).text("Clear").button({
            text: false,
            icons: { primary: 'ui-icon-trash' },
        }).click(function () {
            $('.can-hide', $wrapper).text("");
        });

        $('.icon-keeper-section', $wrapper).text("Keep").button({
            text: false,
            icons: { primary: 'ui-icon-star' },
        }).click(function () {
            $wrapper.toggleClass("keeper-active");
            $wrapper.toggleClass("ui-state-highlight");
        });
    }


    prefsInit();
    cliInit();
    $('#target').focus();
});
