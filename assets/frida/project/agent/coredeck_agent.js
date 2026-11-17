import Java from 'frida-java-bridge';

const hooks = new Map();
let nextId = 1;

function textOf(value) {
    try {
        if (value === null || value === undefined) {
            return 'null';
        }
        return String(value);
    } catch (error) {
        return '<unreadable>';
    }
}

function javaPerform(fn) {
    if (typeof Java.performNow === 'function') {
        return Java.performNow(fn);
    }
    let value;
    let failed;
    Java.perform(() => {
        try {
            value = fn();
        } catch (error) {
            failed = error;
        }
    });
    if (failed) {
        throw failed;
    }
    return value;
}

function listModules() {
    return Process.enumerateModules().map((mod) => ({
        name: mod.name,
        base: mod.base.toString(),
        size: mod.size,
        path: mod.path,
    }));
}

function listExports(name) {
    const mod = Process.findModuleByName(name);
    if (mod === null) {
        throw new Error('Module not found: ' + name);
    }
    const found = typeof mod.enumerateExports === 'function' ? mod.enumerateExports() : [];
    return found.map((item) => ({
        name: item.name,
        address: item.address.toString(),
        type: item.type,
    }));
}

function javaClasses(query) {
    const needle = (query || '').toLowerCase();
    const found = [];
    javaPerform(() => {
        Java.enumerateLoadedClasses({
            onMatch(name) {
                if (found.length >= 200) {
                    return;
                }
                if (!needle || name.toLowerCase().includes(needle)) {
                    found.push(name);
                }
            },
            onComplete() {
            },
        });
    });
    return found;
}

function javaMethods(className) {
    const names = [];
    javaPerform(() => {
        const factory = Java.use(className);
        const declared = factory.class.getDeclaredMethods();
        for (let index = 0; index < declared.length; index++) {
            const name = textOf(declared[index].getName());
            if (!names.includes(name)) {
                names.push(name);
            }
        }
    });
    return names;
}

function hookJava(request) {
    const id = nextId++;
    const className = request.className;
    const methodName = request.symbol;
    javaPerform(() => {
        const factory = Java.use(className);
        const method = factory[methodName];
        if (method === undefined) {
            throw new Error('Method not found: ' + methodName);
        }
        const overloads = method.overloads || [method];
        overloads.forEach((overload) => {
            overload.implementation = function () {
                const args = [];
                if (request.logArgs) {
                    for (let index = 0; index < arguments.length; index++) {
                        args.push(textOf(arguments[index]));
                    }
                }
                let trace = '';
                if (request.backtrace) {
                    try {
                        trace = Thread.backtrace(this.context, Backtracer.ACCURATE)
                            .map(DebugSymbol.fromAddress)
                            .join(' | ');
                    } catch (error) {
                        trace = '';
                    }
                }
                send({
                    kind: 'hook',
                    hookId: id,
                    text: className + '.' + methodName + '(' + args.join(', ') + ')' + (trace ? '\n' + trace : ''),
                });
                return this[methodName].apply(this, arguments);
            };
        });
        hooks.set(id, {
            target: className + '.' + methodName,
            detach() {
                overloads.forEach((overload) => {
                    overload.implementation = null;
                });
            },
        });
    });
    return { id: id, target: className + '.' + methodName };
}

function resolveNative(request) {
    if (request.address) {
        return ptr(request.address);
    }
    if (!request.symbol) {
        throw new Error('A native hook needs a symbol or an address.');
    }
    if (!request.module) {
        return Module.getExportByName(null, request.symbol);
    }
    const mod = Process.findModuleByName(request.module);
    if (mod === null) {
        throw new Error('Module not found: ' + request.module);
    }
    if (typeof mod.getExportByName === 'function') {
        return mod.getExportByName(request.symbol);
    }
    return Module.getExportByName(request.module, request.symbol);
}

function hookNative(request) {
    const id = nextId++;
    const address = resolveNative(request);
    const target = request.address || ((request.module || 'global') + '!' + request.symbol);
    const listener = Interceptor.attach(address, {
        onEnter(args) {
            const shown = [];
            if (request.logArgs) {
                for (let index = 0; index < 4; index++) {
                    shown.push(args[index].toString());
                }
            }
            let trace = '';
            if (request.backtrace) {
                trace = Thread.backtrace(this.context, Backtracer.ACCURATE)
                    .map(DebugSymbol.fromAddress)
                    .join(' | ');
            }
            send({
                kind: 'hook',
                hookId: id,
                text: target + '(' + shown.join(', ') + ')' + (trace ? '\n' + trace : ''),
            });
        },
    });
    hooks.set(id, {
        target: target,
        detach() {
            listener.detach();
        },
    });
    return { id: id, target: target };
}

function scan(request) {
    let base;
    let size;
    if (request.module) {
        const mod = Process.findModuleByName(request.module);
        if (mod === null) {
            throw new Error('Module not found: ' + request.module);
        }
        base = mod.base;
        size = mod.size;
    } else {
        base = ptr(request.start || request.address);
        size = request.size | 0;
    }
    const limit = 16 * 1024 * 1024;
    if (size > limit) {
        size = limit;
    }
    const hits = Memory.scanSync(base, size, request.pattern);
    return hits.slice(0, 64).map((hit) => ({
        address: hit.address.toString(),
        size: hit.size,
    }));
}

function handle(request, data) {
    switch (request.method) {
        case 'modules':
            return { modules: listModules() };
        case 'exports':
            return { exports: listExports(request.module) };
        case 'read':
            return ptr(request.address).readByteArray(Math.max(0, Math.min(request.size | 0, 0x100000)));
        case 'write':
            ptr(request.address).writeByteArray(data);
            return { wrote: data ? data.byteLength : 0 };
        case 'scan':
            return { hits: scan(request) };
        case 'java.classes':
            return { classes: javaClasses(request.query) };
        case 'java.methods':
            return { methods: javaMethods(request.className) };
        case 'hook.java':
            return hookJava(request);
        case 'hook.native':
            return hookNative(request);
        case 'unhook': {
            const hook = hooks.get(request.hookId);
            if (hook) {
                hook.detach();
                hooks.delete(request.hookId);
            }
            return { removed: !!hook };
        }
        case 'hooks':
            return {
                hooks: Array.from(hooks.entries()).map(([id, hook]) => ({
                    id: id,
                    target: hook.target,
                })),
            };
        default:
            throw new Error('Unknown method: ' + request.method);
    }
}

function dispatch(message, data) {
    try {
        const result = handle(message, data || null);
        if (result instanceof ArrayBuffer) {
            send({ id: message.id, ok: true, bytes: true }, result);
        } else {
            send({ id: message.id, ok: true, result: result });
        }
    } catch (error) {
        send({
            id: message.id,
            ok: false,
            error: String(error && error.stack ? error.stack : error),
        });
    }
    recv('coredeck', dispatch);
}

recv('coredeck', dispatch);
