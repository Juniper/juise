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
    function OutputHandler (options) {
        $.clira.buildObject(this, options, {
            loading: "loading...",
            full: [ ],
        }, null);
    }
    $.extend(OutputHandler.prototype, {
        init: function outputReplace ($parent, content) {
            var debug = "";

            if (this.parse && this.debug)
                debug = "<div class='parse-output'>"
                        + this.parse.render() + "</div>";

            var html = "<div class='output-wrapper'>";

            if (this.header)
                html += this.header;
            
            if (this.buildHeader)
                html += this.buildHeader.call(this);
            
            html += debug + "<div class='output-replace'>"
                + content + "</div></div>";

            var $newp = $(html);
            $parent.append($newp);
            var $output = $("div.output-replace", $newp);
            this.output = $output;
            this.parent = $parent;
        },
        loading: function outputLoading () {
            this.init(this.loading);
        },
        replace: function outputReplace (content) {
            this.output.html(content);
        },
        append: function outputAppend (content) {
            this.parse.dbgpr("output: append: full.length " + this.full.length
                             + ", content.length " + content.length);
            this.full.push(content);

            // Turns out that if we continually pass on incoming
            // data, firefox becomes overwhelmed with the work
            // of rendering it into html.  We cheat here by
            // rendering the first piece, and then letting the
            // rest wait until the RPC is complete.  Ideally, there
            // should also be a timer to render what we've got if
            // the output RPC stalls.
            if (full.length <= 2)
                this.output.html(content);
        },
        complete: function outputComplete () {
            this.parse.dbgpr("output: complete");
            this.output.html(this.full.join(""));
        },
    });

    $.clira.outputHandler = function newOutputHandler (options) {
        return new OutputHandler(option);
    }
});
