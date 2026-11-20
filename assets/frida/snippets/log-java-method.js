Java.perform(() => {
    const className = 'android.app.Activity';
    const methodName = 'onResume';
    const type = Java.use(className);
    type[methodName].implementation = function () {
        console.log(className + '.' + methodName);
        return this[methodName]();
    };
});
