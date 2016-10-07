var PouchDb = require("pouchdb");
var db = new PouchDb("hameboss1");

db.info().then(function(info){
    console.log(info);
})
