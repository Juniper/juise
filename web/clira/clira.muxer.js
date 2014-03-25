/*
 * $Id$
 *
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    var mx_id = 0;              // Identify the muxer instance
    var mx_muxid = 0;           // Identify the muxer operation
    var MX_HEADER_SIZE = 31;
    var MX_HEADER_SIZE1 = 32;
    var MX_DUMP_SIZE = 200;
    var MX_HEADER_FIELD = 8;

    function pad (val, width, lfill, rfill) {
        var str = '' + val;

        while (str.length < width) {
            str = lfill + str + rfill;
        }

        return str;
    }
        
    function substr (str, start, len) {
        return str.substring(start, start + len);
    }

    function escape (str) {
        return str
        .replace(/\n/g, "\\n")
        .replace(/\r/g, "\\r")
        .replace(/&/g, "&amp;")
        .replace(/>/g, "&gt;")
        .replace(/</g, "&lt;")
        .replace(/"/g, "&quot;"); // "); // Hack for emacs-mode
    }

    function muxerOpen () {
        $.dbgpr("muxer: open " + this.id);

        var muxer = this;       // In the callback, this == the websocket
        if (muxer.opening)
            return;
        muxer.opening = true;
        muxer.opened = false;

        muxer.ws = new WebSocket(muxer.url);
        muxer.ws.onopen = function (event) {
            $.dbgpr("muxer: WebSocket is now open");
            muxer.opened = true;
            
            if (muxer.pendingMessages) {
                $.dbgpr("muxer: sending pending messages ("
                        + muxer.pendingMessages.length + ")");
                while (muxer.pendingMessages.length > 0) {
                    var message = muxer.pendingMessages.shift();
                    $.dbgpr("wssend: " + message.length
                        + ":: " + message.substring(0, message.length));
                    muxer.ws.send(message);
                }
            }

            if (muxer.onopen)
                muxer.onopen(event);
        }

        muxer.ws.onclose = function (event) {
            forceClose(event, muxer)
        }

        muxer.ws.onmessage = function (event) {
            $.dbgpr("muxer: ws.onmessage (" + muxer.reading
                    + ") [" + event.data.length + "] ["
                    + escape(event.data.substring(1, MX_HEADER_SIZE1)) + "]");

            if (muxer.reading > 0) {
                muxer.data += event.data;
                muxer.reading -= event.data.length;
                if (muxer.reading > 0) {
                    $.dbgpr("muxer: still reading: " + muxer.reading);
                    return;
                }
                $.dbgpr("muxer: done reading");
            } else {
                muxer.data = event.data;
            }

            muxer.onmessage();
        }

        muxer.ws.onerror = function (event) {
            $.dbgpr("muxer: ws.onerror");
            muxer.ws.close();
        }
        
        // Now that the WebSocket connection to mixer is set up, send over a
        // authinit message to let mixer know to use this connection for
        // future auth requests
        muxer.sendMessage(makeMessage("authinit", this.authmuxid));
        $.dbgpr("muxer: auth muxid for this Muxer is " + this.authmuxid);
        muxer.muxMap[this.authmuxid] = {
            muxid: this.authmuxid,
            onauthinit: function (data) {
                muxer.authwebsocketid = data;
                $.dbgpr("muxer: Auth WebSocket id/muxid is: (" +
                        muxer.authwebsocketid + "/" + muxer.authmuxid + ")");
                muxer.runQueue();
            },
            onhostkey: function (data) {
                $.dbgpr("muxer: authmuxid " + muxer.authmuxid + 
                        " received [hostkey] '" + data + "'");
                var response = JSON.parse(data);
                muxer.onhostkey.call(muxer.muxMap[response.muxid],
                    Ember.View.views[response.authdivid], response);
            },
            onpsphrase: function (data) {
                $.dbgpr("muxer: authmuxid " + muxer.authmuxid + 
                        " received [psphrase] '" + data + "'");
                var response = JSON.parse(data);
                muxer.onpsphrase.call(muxer.muxMap[response.muxid],
                    Ember.View.views[response.authdivid], response);
            },
            onpsword: function (data) {
                $.dbgpr("muxer: authmuxid " + muxer.authmuxid + 
                        " received [psword] '" + data + "'");
                var response = JSON.parse(data);
                muxer.onpsword.call(muxer.muxMap[response.muxid],
                    Ember.View.views[response.authdivid], response);
            }
        };
    }

    function forceClose (event, muxer) {
        $.dbgpr("muxer: WebSocket is now closed: " + event.reason);

        var message;
        if (muxer.opening) {
            if (muxer.opened) {
                message = "connection failure";
            } else {
                message = "cannot establish connection";
            }
        } else {
            message = "unknown failure";
        }
        message += " for CLIRA (" + event.target.url + ")";

        if (muxer.onclose)
            muxer.onclose(event, message);

        for (var i = 0; i < muxer.muxMap.length; i++) {
            if (muxer.muxMap[i] && muxer.muxMap[i].onclose)
                muxer.muxMap[i].onclose(event, message);
        }

        muxer.ws = undefined;
        muxer.opening = muxer.opened = false;
        muxer.muxMap = [ ];
    }

    function muxerClose () {
        $.dbgpr("muxer: closing " + this.id);
        if (this.opening)
            this.ws.close();
        this.ws = undefined;
        this.opening = this.opened = false;
    }

    function muxerMessage () {
        $.dbgpr("muxer: message [" + this.data.length + "] ["
                + escape(this.data.substring(1, MX_HEADER_SIZE1)) + "] ["
                + escape(this.data.substring(MX_HEADER_SIZE1, MX_DUMP_SIZE))
                + "]");

        var data = this.data;
        this.data = undefined;
        var next = undefined;
        this.reading = 0;

        for (;;) {
            var dlen = data.length;
            var h1 = substr(data, 0, 4);
            var len = parseInt(substr(data, 4, MX_HEADER_FIELD), 10);
            var op = substr(data, 13, MX_HEADER_FIELD).trim();
            var muxid  = parseInt(substr(data, 22, MX_HEADER_FIELD), 10);

            $.dbgpr("muxer: header [op: " + op + "] [len: " + len
                    + "] [muxid: " + muxid + "]");

            if (h1 != "#01.") {
                $.dbgpr("muxer: bad header: [" + h1 + "]");
                this.ws.close();
            }

            if (len > dlen) {
                this.data = data;
                this.reading = len - dlen;
                return;
            }

            if (len < dlen) {
                next = data.substr(len);
                data = data.substr(0, len);
            }

            var rest = data.substring(data.indexOf("\n") + 1);
            var attr = data.substring(MX_HEADER_SIZE, data.indexOf("\n"));
            var mux = this.muxMap[muxid];
            if (mux) {
                var tag = "on" + op;
                if (mux[tag]) {
                    mux[tag].call(mux, rest, attr);
                } else {
                    $.dbgpr("muxer: unhandled message: [" + tag + "]");
                }

                // "complete" is the last state, so we release the rpc
                if (op == "complete" || op == "error")
                    this.muxMap[muxid] = undefined;
            }

            if (next == undefined) {
                this.reading = 0;
                this.data = undefined;
                return;
            }

            data = next;
            next = undefined;
        }
    }


    function makeMessage (op, muxid, attrs, payload) {
        if (attrs == undefined)
            attrs = "";
        attrs += "\n";
        if (payload == undefined)
            payload = "";

        var len = attrs.length + payload.length + MX_HEADER_SIZE;
        var header = "#01." + pad(len, MX_HEADER_FIELD, '0', '') + "."
	    + pad(op, MX_HEADER_FIELD, '', ' ') + "."
            + pad(muxid, MX_HEADER_FIELD, '0', '') + ".";

        return header + attrs + payload;
    }

    //
    // Perform an RPC.  options include:
    // - target: destination hostname or ip address
    // - onreply: callback function when pieces of the reply arrive
    // - oncomplete: callback function when the reply is complete
    // - onerror: callback function when bad things happen
    //
    function muxerRpc (options) {
        var muxer = this;

        if (muxer.authwebsocketid <= 0) {
            // We have yet to receive a websocket id from our authinit.  This
            // means we're still waiting for mixer to be launched.  Queue this
            // request up.
            options.muxerRpc = true;
            muxer.queueRequest(options);
            return;
        }

        var attrs = "target=\"" + options.target + "\"";
        var muxid = ++mx_muxid;
        var payload = options.payload;
        if (payload == undefined && options.command)
            payload = "<command>" + options.command + "</command>";
        if (options.create == "no")
            attrs += " create=\"no\"";
        if (this.authmuxid !== undefined) {
            attrs += " authmuxid=\"" + this.authmuxid + "\"";
        }
        if (options.div) {
            attrs += " authdivid=\"" + options.div.attr("id") + "\"";
        }

        var op = options.op;
        if (op == undefined)
            op = "rpc";

        options.muxid = muxid;

        this.muxMap[muxid] = options;

        var message = makeMessage(op, muxid, attrs, payload);

        this.sendMessage(message);
    }

    //
    // Queue this request up since we haven't received our authinit data back
    // yet
    //
    function muxerQueueRequest (options)
    {
        this.queue.push(options);
    }

    //
    // Run through our queue (if any)
    //
    function muxerRunQueue ()
    {
        while (this.queue.length) {
            var options = this.queue.shift();
            if (options.muxerRpc){
                this.rpc(options);
            } else if (options.muxerSlax) {
                this.slax(options);
            }
        }
    }

    //
    // Run a .slax script that possibly has jcs:execute or jcs:open calls (use
    // mixer connection)
    // - script: url to the slax script
    // - args: key value arguments to send to the slax script via HTTP
    // - view: view that the slax script should output into
    // - oncomplete: called when script is done being executed
    // - onerror: called when script fails to execute
    // - type: type of slax data to expect (xml, html, text) [optional]
    //
    function muxerSlax (options) {
        var muxer = this;

        if (muxer.authwebsocketid <= 0) {
            // We have yet to receive a websocket id from our authinit.  This
            // means we're still waiting for mixer to be launched.  Queue this
            // request up.
            options.muxerSlax = true;
            muxer.queueRequest(options);
            return;
        }

        $.ajax({
            url: options.script,
            type: 'GET',
            data: options.args,
            dataType: options.type ? options.type : 'html',
            beforeSend: function (xhr) {
                xhr.setRequestHeader('X-Mixer-Auth-Muxer-ID', muxer.authmuxid);
                xhr.setRequestHeader('X-Mixer-Auth-WebSocket-ID', muxer.authwebsocketid);
                xhr.setRequestHeader('X-Mixer-Auth-Div-ID', options.view.$().attr("id"));
            },
            success: function (data, textStatus, jqXHR) {
                if (options.oncomplete) {
                    options.oncomplete(data);
                }
            },
            error: function (jqXHR, textStatus, errorThrown) {
                if (options.onerror) {
                    options.onerror(errorThrown)
                }
            }
        });
    }

    function muxerSendMessage (message) {
        $.dbgpr("wssend: " + message.length
                + ":: " + message.substring(0, message.length));

        if (this.isOpen()) {
            this.ws.send(message);
        } else {
            if (this.opening == false) {
                $.dbgpr("muxer: send forces open");
                this.open();
            }

            if (this.pendingMessages == undefined)
                this.pendingMessages = [ ];
            this.pendingMessages.push(message);
        }
    }

    function muxerError (error) {
        $.dbgpr("muxer: error: " + error);
    }

    function muxerSimpleOp (muxer, options, answer, attrs, op, extra) {
        console.log("muxerSimpleOp:");
        console.log(extra);
        if (extra && extra.reqid) {
            attrs += " reqid=\"" + extra.reqid + "\"";
        }
        var message = makeMessage(op, options.muxid, attrs, answer);

        muxer.sendMessage(message);
    }

    function muxerHostkey (options, answer, extra) {
        muxerSimpleOp(this, options, answer, "", "hostkey", extra);
    }

    function muxerPsPhrase (options, answer, extra) {
        muxerSimpleOp(this, options, answer, "", "psphrase", extra);
    }

    function muxerPsWord (options, answer, extra) {
        muxerSimpleOp(this, options, answer, "", "psword", extra);
    }

    //
    // Muxer: the connection to the mixer daemon, via a websocket.
    // The mixer acts an a proxy, handling all NETCONF traffic and
    // forwarding data as needed.
    //
    // methods include:
    // - rpc: invoke an rpc, handle data as it comes back
    // options include:
    // 
    //
    var MuxerOptions = { }
    function Muxer (options) {
        $.extend(this, MuxerOptions);
        $.extend(this, options);
        this.muxMap = [ ];
        this.id = ++mx_id;
        this.authmuxid = ++mx_muxid;
        this.authwebsocketid = -1;
        this.queue = [ ];
    }

    $.extend(Muxer.prototype, {
        rpc: muxerRpc,
        slax: muxerSlax,
        open: muxerOpen,
        close:  muxerClose,
        onerror: muxerError,
        onmessage: muxerMessage,
        hostkey: muxerHostkey,
        psphrase: muxerPsPhrase,
        psword: muxerPsWord,
        sendMessage: muxerSendMessage,
        queueRequest: muxerQueueRequest,
        runQueue: muxerRunQueue,
        isOpen: function () {
            return (this.ws != undefined
                    && this.ws.readyState == WebSocket.OPEN)
        },

        isOpening: function () {
            return this.opening;
        },
    });

    $.Muxer = function (options) {
        return new Muxer(options);
    }
});
