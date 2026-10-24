var VirtualJoystick = function (opts) {
    opts = opts || {};
    this._container = opts.container || document.body;
    this._strokeStyle = opts.strokeStyle || 'cyan';
    this._stickEl = opts.stickElement || this._buildJoystickStick();
    this._baseEl = opts.baseElement || this._buildJoystickBase();
    this._mouseSupport = opts.mouseSupport !== undefined ? opts.mouseSupport : false;
    this._stationaryBase = opts.stationaryBase || false;
    this._baseX = this._stickX = opts.baseX || 0;
    this._baseY = this._stickY = opts.baseY || 0;
    this._limitStickTravel = opts.limitStickTravel || false;
    this._stickRadius = opts.stickRadius !== undefined ? opts.stickRadius : 100;
    this._useCssTransform = opts.useCssTransform !== undefined ? opts.useCssTransform : false;
    this._container.style.position = 'relative';
    this._container.appendChild(this._baseEl);
    this._baseEl.style.position = 'absolute';
    this._baseEl.style.display = 'none';
    this._container.appendChild(this._stickEl);
    this._stickEl.style.position = 'absolute';
    this._stickEl.style.display = 'none';
    this._pressed = false;
    this._touchIdx = null;
    if (this._stationaryBase === true) {
        this._baseEl.style.display = '';
        this._baseEl.style.left = (this._baseX - this._baseEl.width / 2) + 'px';
        this._baseEl.style.top = (this._baseY - this._baseEl.height / 2) + 'px';
    }
    this._transform = this._useCssTransform ? this._getTransformProperty() : false;
    this._has3d = this._check3D();
    var bind = function (fn, me) { return function () { return fn.apply(me, arguments); }; };
    this._$onTouchStart = bind(this._onTouchStart, this);
    this._$onTouchEnd = bind(this._onTouchEnd, this);
    this._$onTouchMove = bind(this._onTouchMove, this);
    this._container.addEventListener('touchstart', this._$onTouchStart, false);
    this._container.addEventListener('touchend', this._$onTouchEnd, false);
    this._container.addEventListener('touchmove', this._$onTouchMove, false);
    if (this._mouseSupport) {
        this._$onMouseDown = bind(this._onMouseDown, this);
        this._$onMouseUp = bind(this._onMouseUp, this);
        this._$onMouseMove = bind(this._onMouseMove, this);
        this._container.addEventListener('mousedown', this._$onMouseDown, false);
        this._container.addEventListener('mouseup', this._$onMouseUp, false);
        this._container.addEventListener('mousemove', this._$onMouseMove, false);
    }
};

VirtualJoystick.prototype.destroy = function () {
    this._container.removeChild(this._baseEl);
    this._container.removeChild(this._stickEl);
    this._container.removeEventListener('touchstart', this._$onTouchStart, false);
    this._container.removeEventListener('touchend', this._$onTouchEnd, false);
    this._container.removeEventListener('touchmove', this._$onTouchMove, false);
    if (this._mouseSupport) {
        this._container.removeEventListener('mouseup', this._$onMouseUp, false);
        this._container.removeEventListener('mousedown', this._$onMouseDown, false);
        this._container.removeEventListener('mousemove', this._$onMouseMove, false);
    }
};
VirtualJoystick.touchScreenAvailable = function () { return 'createTouch' in document; };
;(function (destObj) {
    destObj.addEventListener = function (event, fct) {
        if (this._events === undefined) this._events = {};
        this._events[event] = this._events[event] || [];
        this._events[event].push(fct);
        return fct;
    };
    destObj.removeEventListener = function (event, fct) {
        if (this._events === undefined) this._events = {};
        if (!(event in this._events)) return;
        this._events[event].splice(this._events[event].indexOf(fct), 1);
    };
    destObj.dispatchEvent = function (event) {
        if (this._events === undefined) this._events = {};
        if (this._events[event] === undefined) return;
        var args = Array.prototype.slice.call(arguments, 1);
        var events = this._events[event].slice();
        for (var i = 0; i < events.length; i++) {
            var result = events[i].apply(this, args);
            if (result !== undefined) return result;
        }
    };
})(VirtualJoystick.prototype);

VirtualJoystick.prototype.deltaX = function () { return this._pressed ? this._stickX - this._baseX : 0; };
VirtualJoystick.prototype.deltaY = function () { return this._pressed ? this._stickY - this._baseY : 0; };
VirtualJoystick.prototype.up = function () { return this._direction(-1, true); };
VirtualJoystick.prototype.down = function () { return this._direction(1, true); };
VirtualJoystick.prototype.right = function () { return this._direction(1, false); };
VirtualJoystick.prototype.left = function () { return this._direction(-1, false); };
VirtualJoystick.prototype._direction = function (sign, vertical) {
    if (!this._pressed) return false;
    var primary = vertical ? this.deltaY() : this.deltaX();
    var secondary = vertical ? this.deltaX() : this.deltaY();
    return sign * primary > 0 && Math.abs(secondary) <= 2 * Math.abs(primary);
};
VirtualJoystick.prototype._onUp = function () {
    this._pressed = false;
    this._stickEl.style.display = 'none';
    if (!this._stationaryBase) {
        this._baseEl.style.display = 'none';
        this._baseX = this._baseY = this._stickX = this._stickY = 0;
    }
};
VirtualJoystick.prototype._onDown = function (x, y) {
    this._pressed = true;
    if (!this._stationaryBase) {
        this._baseX = x; this._baseY = y; this._baseEl.style.display = '';
        this._move(this._baseEl.style, this._baseX - this._baseEl.width / 2, this._baseY - this._baseEl.height / 2);
    }
    this._stickX = x; this._stickY = y; this._limitStick();
    this._stickEl.style.display = '';
    this._move(this._stickEl.style, this._stickX - this._stickEl.width / 2, this._stickY - this._stickEl.height / 2);
};
VirtualJoystick.prototype._onMove = function (x, y) {
    if (!this._pressed) return;
    this._stickX = x; this._stickY = y; this._limitStick();
    this._move(this._stickEl.style, this._stickX - this._stickEl.width / 2, this._stickY - this._stickEl.height / 2);
};
VirtualJoystick.prototype._limitStick = function () {
    if (!this._limitStickTravel) return;
    var dx = this.deltaX(), dy = this.deltaY();
    var distance = Math.sqrt(dx * dx + dy * dy);
    if (distance > this._stickRadius) {
        this._stickX = dx / distance * this._stickRadius + this._baseX;
        this._stickY = dy / distance * this._stickRadius + this._baseY;
    }
};
VirtualJoystick.prototype._onMouseUp = function () { return this._onUp(); };
VirtualJoystick.prototype._onMouseDown = function (event) {
    event.preventDefault(); return this._onDown(event.clientX, event.clientY);
};
VirtualJoystick.prototype._onMouseMove = function (event) { return this._onMove(event.clientX, event.clientY); };
VirtualJoystick.prototype._onTouchStart = function (event) {
    if (this._touchIdx !== null) return;
    if (this.dispatchEvent('touchStartValidation', event) === false) return;
    this.dispatchEvent('touchStart', event); event.preventDefault();
    var touch = event.changedTouches[0]; this._touchIdx = touch.identifier;
    return this._onDown(touch.pageX, touch.pageY);
};
VirtualJoystick.prototype._onTouchEnd = function (event) {
    if (this._touchIdx === null) return;
    this.dispatchEvent('touchEnd', event);
    var list = event.changedTouches, i;
    for (i = 0; i < list.length && list[i].identifier !== this._touchIdx; i++);
    if (i === list.length) return;
    this._touchIdx = null; event.preventDefault(); return this._onUp();
};
VirtualJoystick.prototype._onTouchMove = function (event) {
    if (this._touchIdx === null) return;
    var list = event.changedTouches, i;
    for (i = 0; i < list.length && list[i].identifier !== this._touchIdx; i++);
    if (i === list.length) return;
    event.preventDefault(); return this._onMove(list[i].pageX, list[i].pageY);
};
VirtualJoystick.prototype._buildJoystickBase = function () {
    var canvas = document.createElement('canvas'); canvas.width = 126; canvas.height = 126;
    var ctx = canvas.getContext('2d'); ctx.beginPath(); ctx.strokeStyle = this._strokeStyle; ctx.lineWidth = 6;
    ctx.arc(canvas.width / 2, canvas.width / 2, 40, 0, Math.PI * 2, true); ctx.stroke();
    ctx.beginPath(); ctx.lineWidth = 2; ctx.arc(canvas.width / 2, canvas.width / 2, 60, 0, Math.PI * 2, true); ctx.stroke();
    return canvas;
};
VirtualJoystick.prototype._buildJoystickStick = function () {
    var canvas = document.createElement('canvas'); canvas.width = 86; canvas.height = 86;
    var ctx = canvas.getContext('2d'); ctx.beginPath(); ctx.strokeStyle = this._strokeStyle; ctx.lineWidth = 6;
    ctx.arc(canvas.width / 2, canvas.width / 2, 40, 0, Math.PI * 2, true); ctx.stroke(); return canvas;
};
VirtualJoystick.prototype._move = function (style, x, y) {
    if (this._transform) style[this._transform] = this._has3d ? 'translate3d(' + x + 'px,' + y + 'px, 0)' : 'translate(' + x + 'px,' + y + 'px)';
    else { style.left = x + 'px'; style.top = y + 'px'; }
};
VirtualJoystick.prototype._getTransformProperty = function () {
    var styles = ['webkitTransform', 'MozTransform', 'msTransform', 'OTransform', 'transform'];
    var el = document.createElement('p');
    for (var i = 0; i < styles.length; i++) if (el.style[styles[i]] !== null) return styles[i];
};
VirtualJoystick.prototype._check3D = function () {
    var prop = this._getTransformProperty();
    if (!prop || !window.getComputedStyle) return false;
    var map = { webkitTransform: '-webkit-transform', OTransform: '-o-transform', msTransform: '-ms-transform', MozTransform: '-moz-transform', transform: 'transform' };
    var el = document.createElement('div'); el.style[prop] = 'translate3d(1px,1px,1px)'; document.body.insertBefore(el, null);
    var val = getComputedStyle(el).getPropertyValue(map[prop]); document.body.removeChild(el);
    return val !== null && val.length && val !== 'none';
};