/*
 * $Id$
 *
 * Copyright 2011, Juniper Network Inc, All rights reserved
 * See ../Copyright for more information.
 */

jQuery(function ($) {
    /*
     * dbgpr() is our old-and-trusted debug print function
     */
    $.dbgpr = function () {
        var debug = localStorage['debug'] ? 
                        JSON.parse(localStorage['debug']) : true;

        /* The actual work is pretty trivial */
        if (debug == null || debug)
            $('#debug-log').append(Array.prototype.slice
                                    .call(arguments).join(" ") + "\n");
    }
});
