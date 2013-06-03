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

jQuery(function ($) {
    jQuery.clira.commandFile({
        name: "location",
        handlebarsFile: "location.html",
        commands: [
            {
                command: "show location",
                arguments: [
                    {
                        name: "location",
                        type: "string",
                        nokeyword: true,
                        mandatory: true,
                        multiple_words: true,
                        help: "Name of a location to resolve",
                    },
                ],
                execute: geolocate,
            },
        ],
    });

    function geolocate ($output, cmd, parse, poss) {
        var url = "http://maps.googleapis.com/maps/api/geocode/json";
        var me = parse.possibilities[0];
        
        if (me.data.location) {
            var opts = {
                sensor: "false",
                address: me.data.location,
            }

            $.getJSON(url, opts, function (json) {
                if (json.status == "OK") {
                    var res = json.results[0];
                    var lat = fixed4(res.geometry.location.lat);
                    var lng = fixed4(res.geometry.location.lng);

//                    var template = this.handlebars("geotop");
//                    var html = template(json);

                    var html = "<div>"
                        + "Address is: <span class='geo-address'>"
                        + res.formatted_address + "</span>"
                        + "</div>";
                    html += "<div class='location'>"
                        + "set system location latitude " + lat
                        + " longitude " + lng + "</div>";

                    // Only make the link if there's a map available
                    if (window.google && window.google.maps)
                        html += "<button class='link'></button>";

                    html += "<div class='map-small hidden'></div>";

                    $output.html(html);
                    if (window.google && window.google.maps) {
                        var $map = $("div.map-small", $output);
                        $("button.link", $output).button({
                            label: "Map it",
                            icons: { primary: "ui-icon-flag" },
                        })
                        .click(function (e) {
                            if ($map.hasClass("hidden")) {
                                $map.removeClass("hidden");

                                var map = new GMaps({
                                    div: $map.get(0),
                                    lat: lat,
                                    lng: lng,
                                });
                                map.addMarker({
                                    lat: lat,
                                    lng: lng,
                                    title: me.data.location,
                                });
                            } else
                                $map.addClass("hidden");
                        });
                    }

                } else if (json.status == "ZERO_RESULTS") {
                    $.clira.makeAlert($output,
                                  "Location not found: " + me.data.location);
                } else {
                    $.clira.makeAlert($output, json.status, "geo failure");
                }
            }).fail(function (x, message, err) {
                $.clira.makeAlert($output, message, "geo failure");
            });
        } else 
            $.clira.makeAlert($output, "missing mandatory address");
    }

    function fixed4 (value) {
        return value.toFixed(4);
    }
});
