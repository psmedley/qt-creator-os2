import QtQuick 2.0

Rectangle {
    function foo(k) {
        // k is a unknown value
        var s = ""
        var n = 0
        var N = null
        var u = undefined
        var b = true
        var o = {}

        if (s == s) {}
        if (s == n) {} // 126 15 16
        if (s == N) {} // 325 15 16
        if (s == u) {} // 325 15 16
        if (s == b) {} // 126 15 16
        if (s == o) {} // 126 15 16
        if (s == k) {} // 126 15 16

        if (n == s) {} // 126 15 16
        if (n == n) {}
        if (n == N) {} // 325 15 16
        if (n == u) {} // 325 15 16
        if (n == b) {} // 126 15 16
        if (n == o) {} // 126 15 16
        if (n == k) {} // 126 15 16

        if (N == s) {} // 325 15 16
        if (N == n) {} // 325 15 16
        if (N == N) {}
        if (N == u) {}

        if (N == b) {} // 325 15 16
        if (N == o) {}
        if (N == k) {} // 126 15 16

        if (u == s) {} // 325 15 16
        if (u == n) {} // 325 15 16
        if (u == N) {}
        if (u == u) {}
        if (u == b) {} // 325 15 16
        if (u == o) {}
        if (u == k) {} // 126 15 16

        if (b == s) {} // 126 15 16
        if (b == n) {} // 126 15 16

        if (b == N) {} // 325 15 16
        if (b == u) {} // 325 15 16
        if (b == b) {}
        if (b == o) {} // 126 15 16
        if (b == k) {} // 126 15 16

        if (o == s) {} // 126 15 16
        if (o == n) {} // 126 15 16
        if (o == N) {}
        if (o == u) {}
        if (o == b) {} // 126 15 16
        if (o == o) {}
        if (o == k) {} // 126 15 16

        if (k == s) {} // 126 15 16
        if (k == n) {} // 126 15 16
        if (k == N) {} // 126 15 16
        if (k == u) {} // 126 15 16
        if (k == b) {} // 126 15 16
        if (k == o) {} // 126 15 16
        if (k == k) {} // 126 15 16

        if (s === s) {}
        if (s === n) {} // 325 15 17
        if (s === N) {} // 325 15 17
        if (s === u) {}
        if (s === b) {} // 325 15 17
        if (s === o) {} // 325 15 17
        if (s === k) {}
        if (s !== s) {}
        if (s !== n) {} // 325 15 17
        if (s !== N) {} // 325 15 17
        if (s !== u) {}
        if (s !== b) {} // 325 15 17
        if (s !== o) {} // 325 15 17
        if (s !== k) {}

        if (n === s) {} // 325 15 17
        if (n === n) {}
        if (n === N) {} // 325 15 17
        if (n === u) {}
        if (n === b) {} // 325 15 17
        if (n === o) {} // 325 15 17
        if (n === k) {}

        if (N === s) {} // 325 15 17
        if (N === n) {} // 325 15 17
        if (N === N) {}
        if (N === u) {}

        if (N === b) {} // 325 15 17
        if (N === o) {} // 325 15 17
        if (N === k) {}

        if (u === s) {}
        if (u === n) {}
        if (u === N) {}
        if (u === u) {}
        if (u === b) {}
        if (u === o) {}
        if (u === k) {}

        if (b === s) {} // 325 15 17
        if (b === n) {} // 325 15 17

        if (b === N) {} // 325 15 17
        if (b === u) {}
        if (b === b) {}
        if (b === o) {} // 325 15 17
        if (b === k) {}

        if (o === s) {} // 325 15 17
        if (o === n) {} // 325 15 17
        if (o === N) {} // 325 15 17
        if (o === u) {}
        if (o === b) {} // 325 15 17
        if (o === o) {} // 325 15 17
        if (o === k) {}

        if (k === s) {}
        if (k === n) {}
        if (k === N) {}
        if (k === u) {}
        if (k === b) {}
        if (k === o) {}
        if (k === k) {}

        var nObj = Number("0")
        var nNum = Number.fromLocaleString("0")
        if (n === 1) {}
        if (nObj === 1) {}
        if (nNum === 1) {}

        var bObj = Boolean(1 > 0);
        if (bObj === b) {}

        var sBool = String(b);
        if (sBool === s) {}

    }

    ListView {
        model: ListModel{ id: myModel }
        delegate: Item {
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (index === -1) {}
                }
            }
        }
    }
}
