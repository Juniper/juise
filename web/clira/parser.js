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

//
// The CLIRA parser turns input text into a set of possible valid
// parses, sorted by scores.  Commands are add using the
// $.clira.addCommand() function (typically called from dynamically
// loaded javascript files) and parsing is done using $.clira.parser.
//
//    var parse = $.clira.parse(cmd);
//

jQuery(function ($) {
    if ($.clira == undefined)   // $.clira is our namespace object
        $.clira = { };

    // Command files are dynmically loaded javascript files
    var commandFilenames = [ ];
    var commandFiles = [ ];

    $.extend($.clira, {
        debug: true,           // Have debug output use $.dbgpr()
        commands:  [ ],         // The set of commands we accept
        types: { },             // Set of builtin types
        bundles: { },           // Define a set of bundled arguments
        scoring: {              // Scoring constants
            enumeration: 3,     // When we see an enumeration value
            keyword: 15,        // When we see a keyword
            multiple_words: 0,  // When we add a word to a multi-word arg
            order: 5,           // When the args are in order
            name: 5,            // When we see a match
            name_exact: 10,     // When the match is exact
            needs_data: 5,      // When an argument needs data
            nokeyword: 2,       // When an argument is nokeyword
            missing_keyword: 5  // What we lose when are missing a keyword
        },

        lang: {
            //
            // $.clira.lang: Contains functions and constants needed
            // by the current language.  This will eventually be
            // pluggable as a means of supporting multiple languages.
            //
            match: function langMatch (name, token) {
                // Match a input token against a command token
                if (name.substring(0, token.length) == token)
                    return true;
                else false;
            }
        },
        buildObject: function buildObject (obj, base, defaults, adds) {
            // Simple but flexible means of building an object.  The
            // arguments have different precedences:
            //     adds > base > defaults > obj
            // Typically base is passed to the constructor, base are
            // fields the base can override, and adds are fields that
            // it cannot.

            if (defaults)
                $.extend(obj, defaults);
            if (base)
                $.extend(obj, base);
            if (adds)
                $.extend(obj, adds);
        }
    });

    function Command (options) {
        $.dbgpr("New command: " + options.command);
        $.clira.buildObject(this, options, { arguments: [ ]}, null);

        $.extend(this, options);
        var that = this;

        var toks = splitTokens(options.command);

        while (toks.length > 0) {
            var t = toks.pop();
            this.arguments.unshift({
                name: t,
                type: "keyword"
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

    $.extend($.clira, {
        addCommand: function addCommand (command) {
            if (typeof command == "Command") {
                $.clira.commands.push(command);
            } else if ($.isArray(command)) {
                $.each(command, function (x,c) {
                    $.clira.commands.push(new Command(c));
                });
            } else {
                $.clira.commands.push(new Command(command));
            }
        },
        addType: function addType (type) {
            // A type extends the list of built-in types
            if ($.isArray(type)) {
                $.each(type, function (n, t) {
                    $.clira.types[t.name] = t;
                });
            } else {
                $.clira.types[type.name] = type;
            }
        },
        addBundle: function addBundle (bundle) {
            // A bundle is a set of arguments that can be referenced
            // as a single chunk, for simplicity and consistency
            if ($.isArray(bundle)) {
                $.each(bundle, function (n, b) {
                    $.clira.bundles[b.name] = b;
                });
            } else {
                $.clira.bundles[bundle.name] = bundle;
            }
        },
        loadCommandFiles: function loadCommandFiles () {
            // Load (or reload) the set of command files, which we
            // get from our web server.
            $.ajax("/bin/list-command-files.slax")
                .success(function loadCommandFilesDone (data, status, jqxhr) {

                    if (data.files == undefined || data.files.length == 0) {
                        $.dbgpr("load command files: list is empty, ignored");
                        return;
                    }

                    $.each(commandFiles, function (i, o) {
                        if (o.deinit) {
                            $.dbgpr("commandFileCleanup: calling " + o.name);
                            o.deinit();
                        }
                    });

                    $.clira.commands = [ ];
                    $.clira.loadBuiltins();

                    commandFilenames = [ ];
                    commandFiles = [ ];

                    // Remove all the old command script files
                    $("script.commandFile").remove();
                    $("script.prereq").remove();

                    $.dbgpr("load command files success: " + data.files.length);
                    $.each(data.files, function (i, filename) {
                        $.clira.loadFile(filename, "commandFile");
                    });
                })
                .fail(function loadCommandFilesFail (jqxhr, settings,
                                                     exception) {
                    $.dbgpr("load command files failed");
                });
        },
        loadFile: function loadFile (filename, classname) {
            commandFilenames.push(filename);

            // jQuery's getScript/ajax logic will get a script and
            // eval it, but when there's a problem, you don't get
            // any information about it.  So we use <script>s in the
            // <head> to get 'er done.
            var html = "<scr" + "ipt " + "type='text/javascript'"
                + " class='" + classname + "'"
                + " src='" + filename + "'></scr" + "ipt>";

            if (true) {
                (function() {
                    var ga = document.createElement('script');
                    ga.type = 'text/javascript';
                    ga.setAttribute("class", classname);
                    ga.async = "true";
                    ga.src = filename;
                    var s = document.getElementById('last-script-in-header');
                    s.parentNode.insertBefore(ga, s);
                })();
            } else if (true) {
                $(html).insertBefore("script#last-script-in-header");
            } else if (false) {
                (function () {
                    document.write(html);
                }).call(window);

            } else {
                var $html = $(html);
                var tag = $html.get(0);
                var $last = $("script#last-script-in-header");
                var last = $last.get(0);
                last.parentNode.insertBefore(tag, last);
            }
        },
        onload: function onload (name, data) {
            $.dbgpr("clira: load: " + name);
            if ($.isArray(data)) {
                // We have an array of commands
                $.clira.addCommand(data);
            } else if (typeof data == "function") {
                // We have a callback
                data($);
            } else if (typeof data == "object") {
                if (data.command) {
                    // We have a single command (assumably)
                    $.clira.addCommand(data);
                } else {
                    // We have a set of commands (maybe?)
                    $.each(data, $.clira.addCommand);
                }
            }
        },
        commandToHtml: function commandToHtml (text, full) {
            // Turn command text into a fancy representation

            var p = new Parse(options);
            p.commandText = text; // Only match one command
            p.parse(text);

            p.possibilities.sort(function sortPossibilities (a, b) {
                return b.score - a.score;
            });

            var html = parse.render({ full: true });
        }
    });

    function splitTokens (input) {
        return $.trim(input).split(/\s+/);
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

    function contentsAreEqual(a, b) {
        var equal = (a.length == b.length);
        if (equal) {
            $.each(a, function (k, v) {
                if (a[k] != b[k]) {
                    equal = false;
                    return false;
                }
            });
        }
        return equal;
    }

    function CommandFile (base) {
        $.clira.buildObject(this, base, null, null);
    }
    $.clira.commandFile = function (base) {
        var me = new CommandFile(base);
        if (me.init)
            me.init();
        me.onload();
        commandFiles.push(me);
        return me;
    }
    $.extend(CommandFile.prototype, {
        onload: function () {
            $.dbgpr("clira: load: " + this.name);

            // We have an array of commands
            if (this.commands)
                $.clira.addCommand(this.commands);

            if (this.prereqs) {
                $.each(this.prereqs, function (i, filename) {
                    var $p = $("script.prereq[src = '" + filename + "']");
                    if ($p.length == 0)
                        $.clira.loadFile(filename, "prereq");
                });
            }
        }
    });

    var parse_id = 1;
    function Parse (base) {
        $.clira.buildObject(this, base, null, { id: parse_id++ });
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
        parse: function parse (inputString) {
            // Attempt a parse of the given input string
            this.input = {
                string: inputString,
                tokens:  splitTokens(inputString)
            }
            var that = this;

            // Create a function with the right "that" in scope.
            // We'll pass this function to the various dump() methods.
            function dbgpr() {
                that.dbgpr(Array.prototype.slice.call(arguments).join(" "));
            }

            // Start with a set of empty possibilities, one for each command
            var possibilities = buildInitialPossibilities(this.commandText);

            // For each input token, look for new possibilities
            $.each(this.input.tokens, function (argn, tok) {
                if (tok.length == 0) // Skip drivel
                    return;

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
                    data: tok
                }
                if (poss.last.arg.multiple_words)
                    match.multiple_words = true;

                if (poss.last.arg.enums) {
                    $.each(poss.last.arg.enums, function (n, e) {
                        if ($.clira.lang.match(e.name, tok)) {
                            that.dbgpr("enumeration match for " + e.name);
                            match.enumeration = e;
                            match.data = e.name;
                            that.addPossibility(res, poss, match,
                                                $.clira.scoring.needs_data
                                                + $.clira.scoring.enumeration);
                        }
                    });
                } else {

                    // Add a possibility using this match
                    that.addPossibility(res, poss, match,
                                        $.clira.scoring.needs_data);
                }

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
                if ($.clira.lang.match(arg.name, tok)) {
                    not_seen = false;
                    match = {
                        token: tok,
                        arg: arg
                    }

                    // If the argument needs a data value, then mark it as such
                    if ($.clira.types[arg.type] == undefined)
                        $.dbgpr("unknown type: " + arg.type);
                    if ($.clira.types[arg.type].needs_data)
                        match.needs_data = true;

                    // Calculate the score for this possibility
                    var score = $.clira.scoring.name;
                    if (arg.name.length == tok.length)
                        score += $.clira.scoring.name_exact;
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
                        nokeyword: true
                    }

                    // If the argument allows multiple tokens, mark it as such
                    if (arg.multiple_words)
                        match.multiple_words = true;

                    // Add a possibility using this match
                    that.addPossibility(res, poss, match,
                                        $.clira.scoring.nokeyword);
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
                    multiple_words: true
                }

                // Add a possibility using this match
                that.addPossibility(res, poss, match,
                                    $.clira.scoring.multiple_words);
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
                } else {
                    for (var j = 0; j < i; j++) {
                        var jp = list[j];
                        if (poss.command == jp.command
                                && contentsAreEqual(poss.data, jp.data)) {
                            this.dbgpr("possibility has already been seen");
                            whack = true;
                            break;
                        }
                    }
                }

                for (var k = 0; k < poss.command.arguments.length; k++) {
                    var arg = poss.command.arguments[k];

                    if (arg.type == "keyword" && !poss.seen[arg.name])
                        poss.score -= $.clira.scoring.missing_keyword;
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
        render: function renderParse (opts) {
            var parse = this;
            var res = "<div class='parse'>"
                + "<div class='input-debug'>Input: "
                + parse.input.string + "</div>";
            var that = this;

            parse.eachPossibility(function (x, p) {
                p.render(that, opts);
                res += p.html;
            });

            res += "</div>";

            return res;
        }
    });

    var poss_id = 1;
    function Possibility (base) {
        $.clira.buildObject(this, base, null, { id: poss_id++ });

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
        render: function renderPossibility (parse, opts) {
            var details = "";
            var html = "<div class='possibility'>";
            html += "<div class='command-line'>";
            var poss = this;

            if (opts == undefined)
                opts = { };

            if (opts.details) {
                details = "<div class='details'>Possibility Id: " + this.id
                    + ", Score: " + this.score
                    + ", Command: '" + this.command.command + "'</div>";
                details += "<div class='details'>";
            }

            var emitted = { };

            this.eachMatch(function eachMatch (x, m) {
                var title = "Id: " + m.id + " " + m.arg.name
                    + " (" + m.arg.type + ")";
                if (m.enumeration)
                    title += " (" + m.enumeration.name + ")";
                if (m.data)
                    title += " data";
                if (m.needs_data)
                    title += " needs_data";
                if (m.multiple_words)
                    title += " multiple_words";

                if (opts.details) {
                    details += "<div class='match-details' title='" + title
                        + "'>" + m.token + "</div> ";
                }

                html += emitMissingTokens(poss, m, emitted, false);
                html += "<div class='command-token' title='" + title + "'>";

                var full = m.enumeration ? m.enumeration.name : m.data ? "" : m.arg.name;

                html += commandToken(poss, m, m.token, full);
                html += "</div> ";
            });

            html += emitMissingTokens(this, null, emitted, opts);

            html += "</div>";
            if (opts.details) {
                details += "</div>";
                details += "<div class='details'>Data: {" + dump(this.data)
                    + "}, Seen: {" + dump(this.seen) + "}</div>";
                html += "<div class='parse-details'>" + details + "</div>";
            }
            
            html += "</div>";

            this.html = html;
            var text = renderAsText(html);
            this.text = text;
            return text;
        }
    });

    var match_id = 1;
    function Match (base) {
        $.clira.buildObject(this, base, { },
                    { id: match_id++ });
    }
    $.extend(Match.prototype, {
        dump: function dumpMatch(dbgpr, indent) {
            dbgpr.call(null, indent + "Match: " + this.id
                    + " [" + this.token + "] -> " + this.arg.name
                       + (this.data ? " data" : "")
                       + (this.needs_data ? " needs_data" : "")
                       + (this.multiple_words ? " multiple_words" : ""));
        }
    });

    function buildInitialPossibilities (commandText) {
        var possibilities = [ ];
        $.each($.clira.commands, function (n, c) {
            //
            // If we have a commandText value, then we use that
            // to pick the command to parse against
            //
            if (commandText && c != commandText)
                return;

            var p = new Possibility({
                command: c,
                matches: [ ],
                score: 0
            });
            possibilities.push(p);
        });

        return possibilities;
    }

    $.clira.parse = function parse (inputString, options) {
        var p = new Parse(options);
        p.parse(inputString);
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

    function commandToken(poss, match, token, value) {
        var res = "";
        if (match.nokeyword)
            res = "<div class='parse-implicit-keyword'>"
            + match.arg.name + "</div> ";

        res += "<div class='parse-token'>" + token + "</div>";

        var trailing = value.substring(token.length);
        if (trailing)
            res += "<div class='parse-trailing'>" + trailing + "</div>";

        return res;
    }

    function renderAsText (html) {
        var res = "";
        for (;;) {
            var s = html.indexOf("<");
            if (s < 0)
                break;
            var e = html.indexOf(">", s);
            if (s != 0)
                res += html.substring(0, s);
            html = html.substring(e + 1);
        }
        res += html;
        return res;
    }

    // If there are missing token (keywords) then emit them now.
    function emitMissingTokens (poss, match, emitted, opts) {
        var res = "";
        var all_keywords = true;

        $.each(poss.command.arguments, function (n, arg) {
            // Stop when we hit the current match
            if (match && arg == match.arg)
                return false;

            // If we've already emitted it, skip
            if (emitted[arg.name])
                return true;

            if (!poss.seen[arg.name]) {
                var cl;

                if (arg.mandatory)
                    cl = "mandatory";
                else if (opts.full)
                    cl = "not-seen";

                if (cl) {
                    res += "<div class='parse-" + cl + "'>"
                        + arg.name + "</div> ";
                    res += "<div class='parse-" + cl + "-value'>"
                        + arg.name + "</div> ";
                    emitted[arg.name] = true;
                }
            }

            // If it's a keyword that we haven't seen, emit it
            if (arg.type == "keyword" && !poss.seen[arg.name]) {
                res += "<div class='parse-missing'>" + arg.name + "</div> ";
                emitted[arg.name] = true;
            }
        });

        return res;
    }
});
