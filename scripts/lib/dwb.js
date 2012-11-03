(function() {
    var _modules = {};
    var _requires = {};
    var _callbacks = [];
    var _initialized = false;
    var _applyRequired = function(names, callback) {
      if (names === null) 
        callback.call(this, _modules);
      else {
        var modules = [];
        for (var j=0, m=names.length; j<m; j++) {
          modules.push(_modules[names[j]]);
        }
        callback.apply(this, modules);
      }
    };
    Object.defineProperties(this, { 
        "provide" : { 
          value : function(name, module) {
            if (_modules[name]) 
              io.debug({ 
                  error : new Error("provide: Module " + name + " is already defined!"), 
                  offset : 1, arguments : arguments 
              });
            else 
              _modules[name] = module;
          }
        },
        "require" : {
          value : function(names, callback) {
            if (names !== null && ! (names instanceof Array)) {
              io.debug({ error : new Error("require : invalid argument (" + JSON.stringify(names) + ")"), offset : 1, arguments : arguments });
              return; 
            }

            if (!_initialized) 
              _callbacks.push({callback : callback, names : names});
            else 
              _applyRequired(names, callback);
          }
        },
        // Called after all scripts have been loaded and executed
        // Immediately deleted from the global object, so it is not callable
        // from other scripts
        "_init" : { 
          value : function() {
            _initialized = true;
            for (var i=0, l=_callbacks.length; i<l; i++) 
              _applyRequired(_callbacks[i].names, _callbacks[i].callback);

            Object.freeze(this);
          },
          configurable : true
        }
    });
})();
Object.preventExtensions(this);
