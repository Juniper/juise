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
                templateFile: '/clira/templates/location.hbs',
                arguments: [
                    {
                        name: "location",
                        type: "string",
                        nokeyword: true,
                        mandatory: true,
                        multiple_words: true,
                        help: "Name of a location to resolve"
                    }
                ],
                execute: function(view, cmd, parse, poss) {
                    var output = geolocate(view, parse);
                    if (output.address) {
                        output.map = {
                            title: output.address,
                            height: "200px",
                            lat: output.lat,
                            lng: output.lng
                        }
                    }
                    output.hide = true;

                    view.get('controller').set('output', output);

                    // actions for buttons to handle show/hide map
                    var toggleMap = function() {
                            if (this.get('output').hide) {
                                this.set('output.hide', false);
                            } else {
                                this.set('output.hide', true);
                            }
                    };
                    view.get('controller')._actions.toggleMap = toggleMap;
                }
            }
        ]
    });

    function geolocate (view, parse) {
        var url = "http://maps.googleapis.com/maps/api/geocode/json";
        var me = parse.possibilities[0];
        var output = {};
        
        if (me.data.location) {
            var opts = {
                sensor: "false",
                address: me.data.location
            }

            $.ajax({
                url: url,
                dataType: 'json',
                async: false,
                data: opts,
                success: function(json) {
                    if (json.status == "OK") {
                        var res = json.results[0];
                        var lat = fixed4(res.geometry.location.lat);
                        var lng = fixed4(res.geometry.location.lng);

                        if (window.google && window.google.maps) {
                            output.address = res.formatted_address;
                            output.lat = lat;
                            output.lng = lng;
                        }
                    } else if (json.status == "ZERO_RESULTS") {
                        output.error = {
                            message: "Location not found: " 
                                        + me.data.location,
                            type: "error"
                        };
                    } else {
                        if (json.status) {
                            output.error = {
                                message: "geo failure",
                                type: "error"
                            };
                        } else {
                            output.error = {
                                message: json.status,
                                type: "error"
                            };
                        }
                    }
                },
                fail: function (x, message, err) {
                    if (message) {
                        output.error = {
                            message: message,
                            type: "error"
                        };
                    } else {
                        output.error = {
                            message: "geo failure",
                            type: "error"
                        };
                    }
                }
            });
        } else { 
            output.error = { 
                message: "missing mandatory address", 
                type: "error"
            };
        }

        return output;
    }

    function fixed4 (value) {
        return value.toFixed(4);
    }
});
