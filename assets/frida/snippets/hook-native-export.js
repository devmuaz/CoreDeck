const address = Module.getExportByName(null, 'open');
Interceptor.attach(address, {
    onEnter(args) {
        console.log('open(' + args[0].readUtf8String() + ')');
    },
});
