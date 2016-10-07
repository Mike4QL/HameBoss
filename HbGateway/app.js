var PouchDb = require("pouchdb");
var db = new PouchDb("http://localhost:5984/hameboss1", {
    auth: {
        username: 'admin',
        password: 'brilt'
    }
});

db.info().then(function(info){
    console.log(info);
})
