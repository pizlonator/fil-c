// SPDX-FileCopyrightText: 2021 GNOME Foundation
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// eslint-disable-next-line no-unused-vars
function hasClass(elem, className) {
    return elem && elem.classList && elem.classList.contains(className);
}

// eslint-disable-next-line no-unused-vars
function addClass(elem, className) {
    if (!elem || !elem.classList) {
        return;
    }
    elem.classList.add(className);
}

// eslint-disable-next-line no-unused-vars
function removeClass(elem, className) {
    if (!elem || !elem.classList) {
        return;
    }
    elem.classList.remove(className);
}

function insertAfter(newNode, referenceNode) {
    referenceNode.parentNode.insertBefore(newNode, referenceNode.nextSibling);
}

function onEach(arr, func, reversed) {
    if (arr && arr.length > 0 && func) {
        var length = arr.length;
        var i;
        if (reversed !== true) {
            for (i = 0; i < length; ++i) {
                if (func(arr[i]) === true) {
                    return true;
                }
            }
        } else {
            for (i = length - 1; i >= 0; --i) {
                if (func(arr[i]) === true) {
                    return true;
                }
            }
        }
    }
    return false;
}

function onEachLazy(lazyArray, func, reversed) {
    return onEach(
        Array.prototype.slice.call(lazyArray),
        func,
        reversed);
}

// eslint-disable-next-line no-unused-vars
function hasOwnProperty(obj, property) {
    return Object.prototype.hasOwnProperty.call(obj, property);
}

window.addEventListener("load", function() {
    "use strict;"

    var main = document.getElementById("main");
    var btnToTop = document.getElementById("btn-to-top");

    function labelForToggleButton(isCollapsed) {
        if (isCollapsed) {
            return "+";
        }
        return "\u2212";
    }

    function createToggle(isCollapsed) {
        var toggle = document.createElement("a");
        toggle.href = "javascript:void(0)";
        toggle.className = "collapse-toggle";
        toggle.innerHTML = "[<span class=\"inner\">"
                         + labelForToggleButton(isCollapsed)
                         + "</span>]";

        return toggle;
    }

    function toggleClicked() {
        if (hasClass(this, "collapsed")) {
            removeClass(this, "collapsed");
            this.innerHTML = "[<span class=\"inner\">"
                           + labelForToggleButton(false)
                           + "</span>]";
            onEachLazy(this.parentNode.getElementsByClassName("docblock"), function(e) {
                removeClass(e, "hidden");
            });
        } else {
            addClass(this, "collapsed");
            this.innerHTML = "[<span class=\"inner\">"
                           + labelForToggleButton(true)
                           + "</span>]";
            onEachLazy(this.parentNode.getElementsByClassName("docblock"), function(e) {
                addClass(e, "hidden");
            });
        }
    }

    onEachLazy(document.getElementsByClassName("toggle-wrapper"), function(e) {
        let sectionHeader = e.querySelector(".section-header");
        let fragmentMatches = sectionHeader !== null && location.hash === "#" + sectionHeader.getAttribute('id');
        collapsedByDefault = hasClass(e, "default-hide") && !fragmentMatches;
        var toggle = createToggle(collapsedByDefault);
        toggle.onclick = toggleClicked;
        e.insertBefore(toggle, e.firstChild);
        if (collapsedByDefault) {
            addClass(toggle, "collapsed");
            onEachLazy(e.getElementsByClassName("docblock"), function(d) {
                addClass(d, "hidden");
            });
        }
    });

    function scrollBackTop(e) {
        e.preventDefault();
        window.scroll({
            top: 0,
            behavior: 'smooth',
        });
    }

    function toggleScrollButton() {
        if (window.scrollY < 400) {
            addClass(btnToTop, "hidden");
        } else {
            removeClass(btnToTop, "hidden");
        }
    }

    window.onscroll = toggleScrollButton;
    btnToTop.onclick = scrollBackTop;
}, false);
