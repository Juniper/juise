

jQuery(function ($) {
    $.dbgpr("working");

    var muxer = $.Muxer({
        url: "ws://127.0.0.1:3000/chat",
        onopen: function (event) {
            $.dbgpr("test: opened WebSocket");
        },
        onreply: function (event, data) {
            $.dbgpr("test: onreply: " + data);
        },
        oncomplete: function (event) {
            $.dbgpr("test: complete");
        },
        onclose: function (event) {
            $.dbgpr("test: closed WebSocket");
        }
    });
    muxer.open();

    $.doSend = function (clear) {
        var target = $("#target").val();
        if (target == undefined)
            return;

        var msg = $("#message").val();
        if (!msg.length) {
            return;
        }
        var full = [ ];

        muxer.rpc({
            target: target,
            payload: msg,
            onreply: function (data) {
                $.dbgpr("rpc: reply: full.length " + full.length
                        + ", data.length " + data.length);
                full.push(data);
                // Turns out that if we continually pass on incoming
                // data, firefox becomes overwhelmed with the work
                // of rendering it into html.  We cheat here by
                // rendering the first piece, and then letting the
                // rest wait until the RPC is complete.  Ideally, there
                // should also be a timer to render what we've got if
                // the output RPC stalls.
                if (full.length == 1)
                    $("#content").html(data);
                var $x = full.join("");
            },
            oncomplete: function () {
                $.dbgpr("rpc: complete");
                $("#content").html(full.join(""));
            },
            onhostkey: function (data) {
                muxer.hostkey(this, "yes");
            },
        });

        if (clear)
            $("#message").val("");
    }
})
