/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function($) {
    jQuery.clira.commandFile({
        name: "unit-test-commands",
        commands: [
            {
                command: "show interfaces",
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name"
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type"
                    },
                    {
                        name: "statistics",
                        type: "empty",
                        help: "Show statistics only"
                    },
                    {
                        name: "color",
                        type: "enumeration",
                        help: "Color of interface",
                        enums: [
                            {
                                name: "blue",
                                help: "Blue like the sea after a storm"
                            },
                            {
                                name: "black",
                                help: "Black line the night"
                            },
                            {
                                name: "red",
                                help: "Red"
                            }
                        ]
                    }
                ],
                execute: function () {
                    $.dbgpr("got it");
                }
            },
            {
                command: "show alarms",
                bundle: [ "affecting", "since", "location" ],
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name"
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type"
                    }
                ],
                execute: function () {
                    $.dbgpr("got it");
                }
            },
            {
                command: "show alarms critical extensive",
                arguments: [
                    {
                        name: "interface",
                        type: "interface",
                        help: "Interface name"
                    },
                    {
                        name: "type",
                        type: "media-type",
                        help: "Media type"
                    }
                ],
                execute: function () {
                    $.dbgpr("got it");
                }
            },
            {
                command: "tell",
                arguments: [
                    {
                        name: "user",
                        type: "string",
                        help: "User to send message to",
                        nokeyword: true,
                        mandatory: true
                    },
                    {
                        name: "message",
                        type: "string",
                        multiple_words: true,
                        help: "Message to send to user",
                        nokeyword: true,
                        mandatory: true
                    }
                ],
                execute: function ($output, cmd, parse, poss) {
                    $.dbgpr("got it");
                    $output.html("<div><span>Executing </span>"
                                 + poss.html + "<span> ...</span></div>");
                }
            },
            {
                command: "show outages",
                bundle: [ "location", "since" ]
            },
            {
                command: "map outages",
                bundle: [ "affecting", "since" ]
            },
            {
                command: "list outages",
                bundle: [ "affecting", "since", "between-locations" ]
            },
            {
                command: "list flaps",
                bundle: [ "affecting", "since", "between-locations" ]
            },
            {
                command: "show latency issues",
                bundle: [ "affecting", "since", "location" ]
            },
            {
                command: "show drop issues",
                bundle: [ "affecting", "since" ]
            },
            {
                command: "map paths",
                bundle: [ "between-locations" ]
            },
            {
                command: "list flags",
                bundle: [ "affecting", "since",
                          "between-locations", "location" ]
            },
            {
                command: "test lsp",
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true
                    }
                ]
            },
            {
                command: "route lsp away from device",
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true
                    },
                    {
                        name: "device-name",
                        type: "device",
                        nokeyword: true
                    }
                ]
            },
            {
                command: "configure new lsp",
                bundle: [ "between-devices" ],
                arguments: [
                    {
                        name: "lsp-name",
                        type: "lsp",
                        nokeyword: true
                    }
                ]
            },
            {
                command: "add device to vpn",
                arguments: [
                    {
                        name: "device-name",
                        type: "device",
                        nokeyword: true
                    },
                    {
                        name: "interface",
                        type: "interface"
                    },
                    {
                        name: "vpn-name",
                        type: "vpn"
                    }
                ],
                execute: function ($output, cmd, parse, poss) {
                }
            },
            {
                command: "run parse tests",
                execute: function ($output, cmd, parse, poss) {
                    runParsingTests($output);
                }
            }
        ]
    });

    function runParsingTests ($wrapper) {

        if ($wrapper == undefined)
            $wrapper = $("#output-top");

        $.each(
            [
                "te",
                "tell",
                "tell ",
                "show inter color red",
                "show interfaces color bl",
                "show alarms",
                "alarms ext",
                "complete failure",
                "show al c e",
                "show interfaces type ethernet statistics",
                "show interfaces statistics",
                "tell user phil message now is the time",
                "tell phil this one is working",
                "tell user phil message user security must work",
                "on dent show interfaces fe-0/0/0",
                "show latency issues near iad",
                "show outages near iad",
                "list outages since yesterday",
                "list outages between lax and bos",
                "map paths between lax and bos",
                "list flaps near lax since yesterday",
                "list flaps between device lax and location boston",
                "test lsp foobar",
                "show alarms for northeast",
                "configure new lsp goober between bos and lax",
                "add device bos interface fe-0/0/0 to vpn corporate",
                "route lsp foobar away from device bos",
                "map outages affecting lsp foobar",
                "show latency issues affecting lsp foobar",
                "show drop issues affecting customer blah",
            ], function (x, cmd) {
                $.dbgpr("test: input: [" + cmd + "]");
                var res = $.clira.parse(cmd);
                $.dbgpr("res: " + res.possibilities.length);

                var html = res.render({details: true});
                var $out = $(html);
                $wrapper.append($out);
            }
        );
    }

    function runCommandTests () {
        $.each(
            [
                "tell phil this is working",
            ], function (x, cmd) { $.clira.executeCommand(cmd); }
        );
    }
});
