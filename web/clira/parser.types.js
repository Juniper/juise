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
    $.clira.addType([
        {
            name: "date-and-time",
            needs_data: true,
        },
        {
            name: "device",
            needs_data: true,
        },
        {
            name: "empty",
            needs_data: false,
        },
        {
            name: "enumeration",
            needs_data: true,
        },
        {
            name: "interface",
            needs_data: true,
        },
        {
            name: "location",
            needs_data: true,
        },
        {
            name: "lsp",
            needs_data: true,
        },
        {
            name: "keyword",
            needs_data: false,
            score: $.clira.scoring.keyword,
            order: $.clira.scoring.order,
        },
        {
            name: "media-type",
            needs_data: true,
        },
        {
            name: "string",
            needs_data: true,
        },
        {
            name: "vpn",
            needs_data: true,
        },
    ]);

    $.clira.addBundle([
        {
            name: "location",
            arguments: [
                {
                    name: "near",
                    type: "location",
                },
                {
                    name: "for",
                    type: "location",
                },
            ],
        },
        {
            name: "since",
            arguments: [
                {
                    name: "since",
                    type: "date-and-time",
                },
            ],
        },
        {
            name: "affecting",
            arguments: [
                {
                    name: "affecting",
                    type: "empty",
                },
                {
                    name: "lsp",
                    type: "lsp",
                },
                {
                    name: "customer",
                    type: "string",
                },
            ],
        },
        {
            name: "between-locations",
            arguments: [
                {
                    name: "between",
                    type: "location",
                },
                {
                    name: "and",
                    type: "location",
                },
            ],
        },
        {
            name: "between-devices",
            arguments: [
                {
                    name: "between",
                    type: "device",
                },
                {
                    name: "and",
                    type: "device",
                },
            ],
        },
    ]);
});
