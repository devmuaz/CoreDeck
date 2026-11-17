Interceptor.attach(Module.getExportByName(null, 'open'), {
    onEnter() {
        console.log(
            Thread.backtrace(this.context, Backtracer.ACCURATE)
                .map(DebugSymbol.fromAddress)
                .join('\n')
        );
    },
});
