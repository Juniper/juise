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

jQuery.clira.onload("fancy", [
    {
        command: "show fancy box",

        arguments: [
            {
                name: "color",
                type: "string",
            },
        ],
        execute: function fancyBox ($output, cmd, parse, poss) {
            var color = poss.data.color ? poss.data.color : "#0000ff";
            var svg = "<svg xmlns='http://www.w3.org/2000/svg'\
    xmlns:xlink='http://www.w3.org/1999/xlink'>\
    \
    <rect x='10' y='10' height='110' width='110'\
         style='stroke:#ff0000; fill: " + color + "'>\
    \
        <animateTransform\
            attributeName='transform'\
            begin='0s'\
            dur='20s'\
            type='rotate'\
            from='0 60 60'\
            to='360 60 60'\
            repeatCount='indefinite' \
        />\
    </rect>\
\
</svg>\
";
            $output.html(svg);
        },
    },
]);
