Java.perform(() => {
    const query = 'activity';
    Java.enumerateLoadedClasses({
        onMatch(name) {
            if (name.toLowerCase().includes(query)) {
                console.log(name);
            }
        },
        onComplete() {
        },
    });
});
