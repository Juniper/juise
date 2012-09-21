

jQuery(function ($) {
    $.dbgpr("working");
    var ws;

    if (0) {
        ws = new WebSocket("ws://127.0.0.1:3000/chat");
        if (ws.readyState == ws.CLOSED) {
            $.dbgpr("ws failed");
        } else if (ws.readyState != ws.CLOSED) {
            ws.send("Hello");
            ws.onmessage = function (msg) {
                $.dbgpr("received [" + msg + "]");
            }
        }
    }

    $.doSend = function (clear) {
        var msg = $("#sendmsg").val();
        if (!msg.length) {
            return;
        }
        ws.send(msg);
        if (clear)
            $("#sendmsg").val("");
    }

    ws = new WebSocket("ws://127.0.0.1:3000/chat");
    ws.onopen = function (event) {
        $.dbgpr("opened WebSocket");
    }
    ws.onmessage = function (msg) {
        $("#content").append(msg.data);
    }
    ws.onclose = function (event) {
        $.dbgpr("closed WebSocket");
    }
})

