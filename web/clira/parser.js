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

    $.clira = {
        debug: false,           // Have debug output use $.dbgpr()
    };
    $.clira.commands = [ ];
    $.clira.types = { };
    $.clira.bundles = { };

    var scoring = {
        keyword: 15,
        multiple_words: 0,
        order: 5,
        name: 5,
        name_exact: 10,
        needs_data: 5,
        nokeyword: 2,
        missing_keyword: 5,
    }

    function buildObject (obj, base, defaults, adds) {
        if (defaults)
            $.extend(obj, defaults);
        if (base)
            $.extend(obj, base);
        if (adds)
            $.extend(obj, adds);
    }

    function Command (options) {
        $.dbgpr("New command: " + options.command);
        buildObject(this, options, { arguments: [ ]}, null);

        $.extend(this, options);
        var that = this;

        var toks = splitTokens(options.command);

        while (toks.length > 0) {
            var t = toks.pop();
            this.arguments.unshift({
                name: t,
                type: "keyword",
            });
        }

        if (this.bundle) {
            $.each(this.bundle, function (n, name) {
                if ($.clira.bundles[name] == undefined)
                    $.dbgpr("warning: unknown bundle '" + name
                            + "' for command '" + options.command + "'");
                else {
                    $.each($.clira.bundles[name].arguments, function (n, a) {
                        that.arguments.push(clone(a));
                    });
                }
            });
        }

        if (this.arguments) {
            $.each(this.arguments, function (n, arg) {
                if ($.clira.types[arg.type] == undefined)
                    $.dbgpr("warning: unknown type '" + arg.type
                            + "' for command '" + options.command + "'");
            });
        }
    }

    $.clira.addCommand = function addCommand (command) {
        $.clira.commands.push(new Command(command));
    }

    $.clira.addType = function addType (type) {
        $.clira.types[type.name] = type;
    }

    $.clira.addBundle = function addBundle (bundle) {
        $.clira.bundles[bundle.name] = bundle;
    }

    function splitTokens (input) {
        return input.split(/\s+/);
    }

    function clone (obj) {
        var newObj = (obj instanceof Array) ? [] : {};
        for (i in obj) {
            if (i == 'clone') continue;
            if (obj[i] && typeof obj[i] == "object") {
                newObj[i] = obj[i];
            } else newObj[i] = obj[i]
        }
        return newObj;
    }

    var parse_id = 1;
    function Parse (base) {
        buildObject(this, base, null, { id: parse_id++, });
        this.possibilities = [ ];
        this.debug_log = "";
    }
    $.extend(Parse.prototype, {
        dbgpr: function dbgpr () {
            if ($.clira.debug)
                $.dbgpr(Array.prototype.slice.call(arguments).join(" "));
            else
                this.debug_log += Array.prototype.slice
                    .call(arguments).join(" ") + "\n";
        },
        execute: function parseExecute (inputString) {
            // Execute a parse of the given input string
            this.input = {
                string: inputString,
                tokens:  splitTokens(inputString),
            }
            var that = this;

            // Create a function with the right "that" in scope.
            // We'll pass this function to the various dump() methods.
            function dbgpr() {
                that.dbgpr(Array.prototype.slice.call(arguments).join(" "));
            }

            // Start with a set of empty possibilities, one for each command
            var possibilities = buildInitialPossibilities();

            // For each input token, look for new possibilities
            $.each(this.input.tokens, function (argn, tok) {
                that.dump(dbgpr, possibilities, "[top] >> ",
                           "top of parse loop for token: '" + tok + "'");

                prev = possibilities;
                possibilities = [ ];

                $.each(prev, function (n, poss) {
                    poss.dump(dbgpr, "[exp] >> ");
                    var newposs = that.newPossibilities(poss, tok, argn);
                    if (newposs && newposs.length > 0) {
                        $.each(newposs, function (n, p) {
                            possibilities.push(p);
                            p.dump(dbgpr, "[new] >> ");
                        });
                    }
                });
            });

            this.possibilities = this.postProcess(possibilities);

            this.dump(dbgpr, this.possibilities,
                      "[pro] >> ", "post process");

            return possibilities.length != 0;
        },
        newPossibilities: function newPossibilities (poss, tok, argn) {
            // newPossibilities: look at the current possibility and the next
            // input token and see if there any possible parses.
            var res = [ ];
            var match;
            var not_seen = true;
            var that = this;

            // Three ways to match a token:
            // - leaf name
            // - value for last leaf
            // - more data for last leaf

            // Check if the previous leaf needs a value
            if (poss.last && poss.last.needs_data) {
                this.dbgpr("argument needs data: " + poss.last.arg.name);
                match = {
                    token: tok,
                    arg: poss.last.arg,
                    data: tok,
                }
                if (poss.last.arg.multiple_words)
                    match.multiple_words = true;

                // Add a possibility using this match
                that.addPossibility(res, poss, match, scoring.needs_data);

                // Early return, meaning that data for needs_data won't match
                // normal tokens, which seems reasonable.

                return res;
            }

            // Check for a simple leaf name match
            // Look at each of the command's arguments for a match
            $.each(poss.command.arguments, function (cmdn, arg) {
                // If the argument has been seen already, skip it
                if (poss.seen[arg.name])
                    return;

                // If the name matches the input token, we have a
                // possible parse
                if (arg.name.substring(0, tok.length) == tok) {
                    not_seen = false;
                    match = {
                        token: tok,
                        arg: arg,
                    }

                    // If the argument needs a data value, then mark it as such
                    if ($.clira.types[arg.type] == undefined)
                        $.dbgpr("unknown type: " + arg.type);
                    if ($.clira.types[arg.type].needs_data)
                        match.needs_data = true;

                    // Calculate the score for this possibility
                    var score = scoring.name;
                    if (arg.name.length == tok.length)
                        score += scoring.name_exact;
                    if (argn == cmdn && $.clira.types[arg.type].order)
                        score += $.clira.types[arg.type].order;

                    // Add a possibility using this match
                    that.addPossibility(res, poss, match, score);
                }

                // Check if the argument is nokeyword and that we haven't
                // seen a keyworded match.
                if (arg.nokeyword && not_seen) {
                    if (poss.seen[arg.name]) // Seen this arg already?
                        return;

                    // Force order onto nokeywords, so that one
                    // can only be seen if all nokeyword arguments
                    // defined previously in the command have values.
                    if (poss.missingPreviousNokeywords(arg))
                        return;

                    match = {
                        token: tok,
                        arg: arg,
                        data: tok,
                    }

                    // If the argument allows multiple tokens, mark it as such
                    if (arg.multiple_words)
                        match.multiple_words = true;

                    // Add a possibility using this match
                    that.addPossibility(res, poss, match, scoring.nokeyword);
                }
            });

            // Check if the previous leaf allows multiple values
            if (poss.last && poss.last.multiple_words) {
                this.dbgpr("argument allows multiple words: "
                        + poss.last.arg.name);
                match = {
                    token: tok,
                    arg: poss.last.arg,
                    data: tok,
                    multiple_words: true,
                }

                // Add a possibility using this match
                that.addPossibility(res, poss, match, scoring.multiple_words);
            }

            return res;
        },
        addPossibility: function addPossibility (possibilities, base,
                                                 match_options, score) {
            // Create a new possible parse using the base (previous) parse
            // and the options provided.  Then add that to the list
            // of possibilities.

            var that = this;

            var poss = new Possibility(base);
            poss.score += score;
            if ($.clira.types[match_options.arg.type].score)
                poss.score += $.clira.types[match_options.arg.type].score;

            var match = new Match(match_options);
            poss.addMatch(match);
            poss.last = match;
            if (match.data) {
                if (poss.data[match.arg.name] == undefined)
                    poss.data[match.arg.name] = match.data;
                else
                    poss.data[match.arg.name] += " " + match.data;
            }

            possibilities.push(poss);
            return poss;
        },
        postProcess: function postProcess (list) {
            // We now remove matches that simply don't make sense.
            for (var i = 0; i < list.length; i++) {
                var poss = list[i];
                var whack = false;
                var seen_non_nokeyword = false;

                for (var j = 0; j < poss.matches.length; j++) {
                    var match = poss.matches[j];
                    if (!match.arg.nokeyword)
                        seen_non_nokeyword = true;
                }

                if (!seen_non_nokeyword) {
                    this.dbgpr("all matches are nokeyword; whacking: "
                               + poss.id + ": " + poss.command.command);
                    whack = true;
                }

                for (var k = 0; k < poss.command.arguments.length; k++) {
                    var arg = poss.command.arguments[k];

                    if (arg.type == "keyword" && !poss.seen[arg.name])
                        poss.score -= scoring.missing_keyword;
                }

                // Remove possibilities where all matches are nokeyword
                if (whack)
                    list.splice(i--, 1);
            }

            return list;
        },
        dump: function parseDump (dbgpr, list, indent, tag) {
            dbgpr.call(null, tag);
            var that = this;
            $.each(list, function (n, poss) {
                if (poss.dump)
                    poss.dump(dbgpr, indent);
                else
                    dbgpr.call(null, "invalid poss in possibilities");
            });
        },
        eachPossibility: function eachPossibility (fn) {
            $.each(this.possibilities, function (n, p) {
                fn.call(this, n, p);
            });
        },
    });

    var poss_id = 1;
    function Possibility (base) {
        buildObject(this, base, null, { id: poss_id++, });

        /* We need our own copy of the matches and data */
        this.matches = clone(base.matches);
        this.data = clone(base.data);
        this.seen = clone(base.seen);
    }
    $.extend(Possibility.prototype, {
        dump: function (dbgpr, indent) {
            dbgpr.call(null, indent + "Possibility: " + this.id
                       + " command [" + this.command.command + "] "
                       + this.score);
            $.each(this.matches, function (x, m) {
                m.dump(dbgpr, indent + "  ");
            });
            if (this.data)
                dbgpr.call(null, indent
                           + "   Data: {" + dump(this.data) + "}");
            if (this.seen)
                dbgpr.call(null, indent
                           + "   Seen: {" + dump(this.seen) + "}");
        },
        addMatch: function addMatch (match) {
            this.seen[match.arg.name] = true;
            this.matches.push(match);
        },
        allMatchesAreNokeyword: function allMatchesAreNokeyword () {
            for (var i = 0; i < this.matches.length; i++)
                if (!this.matches[i].arg.nokeyword)
                    return false;
            return true;
        },
        missingPreviousNokeywords: function missingPreviousNokeywords (arg) {
            // See if any nokeyword arguments defined before 'arg'
            // in the current command are missing.
            for (var i = 0; i < this.command.arguments.length; i++) {
                var a = this.command.arguments[i];
                if (a.name == arg.name)
                    break;
                if (a.nokeyword && this.seen[a.name] == undefined)
                    return true;
            }
            return false;
        },
        eachMatch: function eachMatch (fn) {
            $.each(this.matches, function eachMatchCb (n, m) {
                fn.call(this, n, m);
            });
        },
        eachData: function eachData (fn) {
            $.each(this.data, function eachDataCb (n, m) {
                fn.call(this, n, m);
            });
        },
        eachSeen: function eachSeen (fn) {
            $.each(this.seen, function eachSeenCb (n, m) {
                fn.call(this, n, m);
            });
        },
    });

    var match_id = 1;
    function Match (base) {
        buildObject(this, base, { },
                    { id: match_id++, });
    }
    $.extend(Match.prototype, {
        dump: function dumpMatch(dbgpr, indent) {
            dbgpr.call(null, indent + "Match: " + this.id
                    + " [" + this.token + "] -> " + this.arg.name
                       + (this.data ? " data" : "")
                       + (this.needs_data ? " needs_data" : "")
                       + (this.multiple_words ? " multiple_words" : ""));
        },
    });

    function buildInitialPossibilities () {
        var possibilities = [ ];
        $.each($.clira.commands, function (n, c) {
            var p = new Possibility({
                command: c,
                matches: [ ],
                score: 0,
            });
            possibilities.push(p);
        });
        return possibilities;
    }

    function parse (inputString, options) {
        var p = new Parse(options);
        p.execute(inputString);
        p.possibilities.sort(function sortPossibilities (a, b) {
            return b.score - a.score;
        });
        return p;
    }

    function dump (obj) {
        var s = " ";
        $.each(obj, function (key, value) {
            s += "'" + key + "': '" + value + "', ";
        });
        return s;
    }

    function load () {
        $.each(
            [
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
                    score: scoring.keyword,
                    order: scoring.order,
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
            ], function (x, t) { $.clira.addType(t);});

        $.each(
            [
                {
                    name: "location",
                    arguments: [
                        {
                            name: "near",
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
            ], function (x, o) { $.clira.addBundle(o); });

        $.each(
            [
                {
                    command: "show interfaces",
                    arguments: [
                        {
                            name: "interface",
                            type: "interface",
                            help: "Interface name",
                        },
                        {
                            name: "type",
                            type: "media-type",
                            help: "Media type",
                        },
                        {
                            name: "statistics",
                            type: "empty",
                            help: "Show statistics only",
                        },
                    ],
                    execute: function () {
                        $.dbgpr("got it");
                    },
                },
                {
                    command: "show alarms",
                    bundle: [ "affecting", "since", "location", ],
                    arguments: [
                        {
                            name: "interface",
                            type: "interface",
                            help: "Interface name",
                        },
                        {
                            name: "type",
                            type: "media-type",
                            help: "Media type",
                        },
                    ],
                    execute: function () {
                        $.dbgpr("got it");
                    },
                },
                {
                    command: "show alarms critical extensive",
                    arguments: [
                        {
                            name: "interface",
                            type: "interface",
                            help: "Interface name",
                        },
                        {
                            name: "type",
                            type: "media-type",
                            help: "Media type",
                        },
                    ],
                    execute: function () {
                        $.dbgpr("got it");
                    },
                },
                {
                    command: "tell",
                    arguments: [
                        {
                            name: "user",
                            type: "string",
                            help: "User to send message to",
                            nokeyword: true,
                        },
                        {
                            name: "message",
                            type: "string",
                            multiple_words: true,
                            help: "Message to send to user",
                            nokeyword: true,
                        },
                    ],
                    execute: function () {
                        $.dbgpr("got it");
                    },
                },
                {
                    command: "on",
                    arguments: [
                        {
                            name: "target",
                            type: "string",
                            help: "Remote device name",
                            nokeyword: true,
                        },
                        {
                            name: "command",
                            type: "string",
                            multiple_words: true,
                            help: "Command to execute",
                            nokeyword: true,
                        },
                    ],
                    execute: function () {
                        $.dbgpr("got it");
                    },
                },
                {
                    command: "show latency issues",
                    bundle: [ "location", ],
                },
                {
                    command: "show outages",
                    bundle: [ "location", "since", ],
                },
                {
                    command: "map outages",
                    bundle: [ "affecting", "since", ],
                },
                {
                    command: "list outages",
                    bundle: [ "affecting", "since", ],
                },
                {
                    command: "show latency issues",
                    bundle: [ "affecting", "since", ],
                },
                {
                    command: "show drop issues",
                    bundle: [ "affecting", "since", ],
                },
                {
                    command: "map paths",
                    bundle: [ "between-locations", ],
                },
                {
                    command: "list flags",
                    bundle: [ "affecting", "since",
                              "between-locations", "location", ],
                },
                {
                    command: "test lsp",
                    arguments: [
                        {
                            name: "lsp-name",
                            type: "lsp",
                            nokeyword: true,
                        },
                    ],
                },
                {
                    command: "route lsp",
                    arguments: [
                        {
                            name: "away from device",
                            type: "device",
                        },
                    ],
                },
                {
                    command: "configure new lsp",
                    bundle: [ "between-devices", ],
                    arguments: [
                        {
                            name: "lsp-name",
                            type: "lsp",
                            nokeyword: true,
                        },
                    ],
                },
                {
                    command: "add device to vpn",
                    arguments: [
                        {
                            name: "device-name",
                            type: "device",
                            nokeyword: true,
                        },
                        {
                            name: "interface",
                            type: "interface",
                        },
                        {
                            name: "vpn-name",
                            type: "vpn",
                        },
                    ],
                },

            ], function (x, c) { $.clira.addCommand(c); });
    }

    load();
    var examples = [
    ];
    $.each(
        [
            "show alarms",
            "complete failure",
            "show al c e",
            "show interfaces type ethernet statistics",
            "show interfaces statistics",
            "tell user phil message now is the time",
            "tell user phil message user security must work",
            "on dent show interfaces fe-0/0/0",
            "show latency issues near iad",
            "show outages near iad",
            "map outages affecting lsp foobar",
            "list outages since yesterday",
            "list outages between lax and bos",
            "show latency issues affecting lsp foobar",
            "show drop issues affecting customer blah",
            "map paths between lax and bos",
            "list flaps near lax since yesterday",
            "list flaps between device lax and location boston",
            "test lsp foobar",
            "show alarms for northeast",
            "route lsp foobar away from device bos",
            "configure new lsp goober between bos and lax",
            "add device bos interface fe-0/0/0 to vpn corporate",
        ], function (x, cmd) {
            $.dbgpr("test: input: [" + cmd + "]");
            var res = parse(cmd);
            $.dbgpr("res: " + res.possibilities.length);
            var html = "<div class='parse'>"
                + "<div class='input'>Input: " + cmd + "</div>";
            res.eachPossibility(function (x, p) {
                p.dump($.dbgpr, "results: ");
                html += "<div class='possibility'>";
                html += "<div class='details'>Id: " + p.id
                    + ", Score: " + p.score
                    + ", Command: '" + p.command.command + "'</div>";
                p.eachMatch(function eachMatch (x, m) {
                    var title = "Id: " + m.id + " " + m.arg.name
                        + " (" + m.arg.type + ")";
                    if (this.data)
                        title += " data";
                    if (this.needs_data)
                        title += " needs_data";
                    if (this.multiple_words)
                        title += " multiple_words";
                    html += "<div class='match' title='" + title + "'>";
                    html += m.token;
                    html += "</div>";
                });
                html += "<div class='details'>Data: {" + dump(p.data)
                    + "}, Seen: {" + dump(p.seen) + "}</div>";
                html += "</div>";
            });
            html += "</div>";
            var $out = $(html);
            $("#output").append($out);
        }
    );
});
