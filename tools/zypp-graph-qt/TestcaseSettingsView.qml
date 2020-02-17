import QtQuick 2.9
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtQuick.Layouts 1.13

ScrollView {
    padding: 10

    GridLayout {

        columns: 2

        Label {
            text: "Solver Focus"
        }
        ComboBox {
            id: focusComboBox
            model: testcaseData.resolverFocusModes()
            Layout.fillWidth: true
            onCurrentIndexChanged: {
                testcaseData.setResolverFocusFromString( currentText );
            }
            function selectByName ( name ) {
                var index = find(name);
                if ( index === -1 )
                    return;
                currentIndex = index;
            }

            Connections {
                target: testcaseData
                onResolverFocusChanged: {
                    focusComboBox.selectByName(resolverFocus);
                }
            }
            Component.onCompleted: {
                focusComboBox.selectByName( testcaseData.resolverFocus )
            }
        }

        Label {
            text: "Hardware Info"
        }
        TextField {
            id: textInputHwInfo
            text: testcaseData.hardwareInfo
            Layout.fillWidth: true
            Binding {
                target: testcaseData
                property: "hardwareInfo"
                value: textInputHwInfo.text
            }
        }

        Label {
            text: "Systemcheck"
        }
        TextField {
            id: textInputSysCheck
            text: testcaseData.systemCheck
            Layout.fillWidth: true
            Binding {
                target: testcaseData
                property: "systemCheck"
                value: textInputSysCheck.text
            }
        }

        Label {
            text: "Solver Options"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            GridLayout {
                columns: 4
                CheckBox {
                    id: checkboxIgnorealreadyrecommended
                    text: "Ignore already recommended"
                    checked: testcaseData.ignorealreadyrecommended
                    Binding {
                        target: testcaseData
                        property: "ignorealreadyrecommended"
                        value: checkboxIgnorealreadyrecommended.checked
                    }
                }
                CheckBox {
                    id: checkboxOnlyRequires
                    text: "Only Requires"
                    checked: testcaseData.onlyRequires
                    Binding {
                        target: testcaseData
                        property: "onlyRequires"
                        value: checkboxOnlyRequires.checked
                    }
                }
                CheckBox {
                    id: checkboxForceResolve
                    text: "Force Resolve"
                    checked: testcaseData.forceResolve
                    Binding {
                        target: testcaseData
                        property: "forceResolve"
                        value: checkboxForceResolve.checked
                    }
                }
                CheckBox {
                    id: checkboxCleandepsOnRemove
                    text: "Clean deps on remove"
                    checked: testcaseData.cleandepsOnRemove
                    Binding {
                        target: testcaseData
                        property: "cleandepsOnRemove"
                        value: checkboxCleandepsOnRemove.checked
                    }
                }
                CheckBox {
                    id: checkBoxAllowDowngrade
                    text: "Allow Downgrade"
                    checked: testcaseData.allowDowngrade
                    Binding {
                        target: testcaseData
                        property: "allowDowngrade"
                        value: checkBoxAllowDowngrade.checked
                    }
                }
                CheckBox {
                    id: checkboxAllowNameChange
                    text: "Allow name change"
                    checked: testcaseData.allowNameChange
                    Binding {
                        target: testcaseData
                        property: "allowNameChange"
                        value: checkboxAllowNameChange.checked
                    }
                }
                CheckBox {
                    id: checkboxAllowArchChange
                    text: "Allow arch change"
                    checked: testcaseData.allowArchChange
                    Binding {
                        target: testcaseData
                        property: "allowArchChange"
                        value: checkboxAllowArchChange.checked
                    }
                }
                CheckBox {
                    id: checkboxAllowVendorChange
                    text: "Allow vendorchange"
                    checked: testcaseData.allowVendorChange
                    Binding {
                        target: testcaseData
                        property: "allowVendorChange"
                        value: checkboxAllowVendorChange.checked
                    }
                }
                CheckBox {
                    id: checklBoxDupAllowDowngrade
                    text: "Dup allow Downgrade"
                    checked: testcaseData.dupAllowDowngrade
                    Binding {
                        target: testcaseData
                        property: "dupAllowDowngrade"
                        value: checklBoxDupAllowDowngrade.checked
                    }
                }
                CheckBox {
                    id: checkboxDupAllowNameChange
                    text: "Dup allow name change"
                    checked: testcaseData.dupAllowNameChange
                    Binding {
                        target: testcaseData
                        property: "dupAllowNameChange"
                        value: checkboxDupAllowNameChange.checked
                    }
                }
                CheckBox {
                    id: checkboxDupAllowArchChange
                    text: "Dup allow arch change"
                    checked: testcaseData.dupAllowArchChange
                    Binding {
                        target: testcaseData
                        property: "dupAllowArchChange"
                        value: checkboxDupAllowArchChange.checked
                    }
                }
                CheckBox {
                    id: checkboxDupAllowVendorChange
                    text: "Dup allow vendorchange"
                    checked: testcaseData.dupAllowVendorChange
                    Binding {
                        target: testcaseData
                        property: "dupAllowVendorChange"
                        value: checkboxDupAllowVendorChange.checked
                    }
                }

                CheckBox {
                    id: checkboxShowMediaId
                    text: "Show Media ID"
                    checked: testcaseData.showMediaId
                    Binding {
                        target: testcaseData
                        property: "showMediaId"
                        value: checkboxShowMediaId.checked
                    }
                }
                CheckBox {
                    id: checkboxLicencebit
                    text: "Only Requires"
                    checked: testcaseData.licencebit
                    Binding {
                        target: testcaseData
                        property: "licencebit"
                        value: checkboxLicencebit.checked
                    }
                }
            }
        } //Solver Options GroupBox

        Label {
            text: "Modalias"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            Component {
                id: modaliasDelegate
                Item {
                    height: 20;
                    width: implicitWidth
                    Column {
                        Text { text: '<b>Name:</b> ' + modelData }
                    }
                }
            }

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.modaliaslist
                    delegate: modaliasDelegate
                }

            }
        }

        Label {
            text: "Multiversion"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.multiversionspec
                    delegate: modaliasDelegate
                }

            }
        }

        Label {
            text: "Autoinst"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.autoInstall
                    delegate: modaliasDelegate
                }

            }
        }

        Label {
            text: "Locales"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            Component {
                id: localeDelegate
                Item {
                    height: 20;
                    width: implicitWidth
                    Column {
                        Text { text: '<b>Name:</b> ' +  name}
                        Text { text: '<b>State:</b> ' + state }
                    }
                }
            }

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.locales
                    delegate: localeDelegate
                }

            }
        }

        Label {
            text: "Sources"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            Component {
                id: sourcesDelegate
                Item {
                    height: 20;
                    width: implicitWidth
                    Column {
                        Text { text: '<b>Alias:</b> ' +  alias}
                        Text { text: '<b>URL:</b> ' + url }
                    }
                }
            }

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.sources
                    delegate: sourcesDelegate
                }

            }
        }

        Label {
            text: "Channels"
            Layout.alignment: Qt.AlignTop
        }
        GroupBox {
            Layout.fillWidth: true
            Layout.minimumHeight: 150
            Layout.maximumHeight: 150

            Component {
                id: channelsDelegate
                Item {
                    height: 20;
                    width: implicitWidth
                    Column {
                        Text { text: '<b>Name:</b> ' + name }
                        Text { text: '<b>File:</b> ' + file }
                        Text { text: '<b>Type:</b> ' + type }
                        Text { text: '<b>Priority:</b> ' + priority }
                    }
                }
            }

            ScrollView {
                anchors.fill: parent
                clip: true
                ListView {
                    model: testcaseData.channels
                    delegate: channelsDelegate
                }

            }
        }


    } //GridLayout
}
