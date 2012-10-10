

jQuery(function ($) {
    var id_generator = 1;

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
        $.dbgpr("doSend: enter");
        var target = $("#target").val();
        if (target == undefined)
            return;

        var msg = $("#message").val();
        if (!msg.length) {
            return;
        }
        var full = [ ];

        var id = id_generator++;
        var content = "<div id='" + id + "' class='test-output'></div>";
        var $div = $(content);
        $("#content").prepend($div);

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
                    $div.html(data);
                var $x = full.join("");
            },
            oncomplete: function () {
                $.dbgpr("rpc: complete");
                $div.html(full.join(""));
            },
            onhostkey: function (data) {
                var self = this;
                promptForHostKey($div, data, function (response) {
                    muxer.hostkey(self, response);
                });
            },
            onpsphrase: function (data) {
                var self = this;
                promptForSecret($div, data, function (response) {
                    muxer.psphrase(self, response);
                });
            },
            onpsword: function (data) {
                var self = this;
                promptForSecret($div, data, function (response) {
                    muxer.psword(self, response);
                });
            },
        });

        if (clear)
            $("#message").val("");
        $.dbgpr("doSend: exit");
    }

    function promptForHostKey ($parent, prompt, onclick) {
        var content = "<div class='muxer-prompt'>"
            + "<div class='muxer-message'>" + prompt + "</div>"
            + "<div class='muxer-buttons'>" 
            +   "<button class='accept'/>"
            +   "<button class='decline'/>"
            + "</div></div>";

        var $div = $(content);
        $parent.append($div);
        $(".accept", $div).text("Accept").button({}).click(function () {
            onclick("yes");
            $div.remove();
        });
        $(".decline", $div).text("Decline").button({}).click(function () {
            onclick("no");
            $div.remove();
        });
    }

    function promptForSecret ($parent, prompt, onclick) {
        var content = "<div class='muxer-prompt'>"
            + "<div class='muxer-message'>" + prompt + "</div>"
            + "<input name='value' type='password' class='value'></input>'"
            + "<div class='muxer-buttons'>" 
            +   "<button class='enter'/>"
            +   "<button class='cancel'/>"
            + "</div></div>";

        var $div = $(content);
        $parent.append($div);
        $(".enter", $div).text("Enter").button({}).click(function () {
            var val = $(".value", $div).val();
            onclick(val);
            $div.remove();
        });
        $(".cancel", $div).text("Cancel").button({}).click(function () {
            var val = $(".value", $div).val();
            onclick(val);
            $div.remove();
        });
    }
})
