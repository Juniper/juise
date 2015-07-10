Clira.rootElement = "#ember-testing";

Clira.setupForTesting();

Clira.injectTestHelpers();

function executeCommand (command) {
    fillIn('#command-input', command);
    click('button.ui-button');
}
