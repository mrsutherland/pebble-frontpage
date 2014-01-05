Pebble.addEventListener("ready",
  function(e) {
    App.post.get();
  }
);

Pebble.addEventListener("showConfiguration",
  function(e) {
    console.log("Showing Configuration");
    Pebble.openURL("html/configuration.html");
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var configuration = JSON.parse(e.response);
    console.log("Configuration window returned: ", configuration);
  }
);

Pebble.addEventListener("appmessage",
  function(e) {
    console.log("Received message: " + e.payload);
    if ("req_post_data" in e.payload) {
      var index = e.payload.index;
      var post = App.post.posts[index];
      if (post == undefined) {
        console.error("Requested data for post index that doesn't exist: "+index);
      }
      // TODO: store content for webpage
      // look for selftext in post using index
      if (post.selftext) {
        // return selftext
        console.log("Returning selftext for post: "+post.title);
        var data = {"post_data": 1,
                    "index": index,
                    "data": post.selftext.slice(0, 1024)};
        App.data.add_to_queue(data);
      } else if (post.domain == "imgur.com" || post.domain == "i.imgur.com") {
        console.log("imgur post...");
        // TODO: show thumbnail
        var data = {"post_data": 1,
                    "index": index,
                    "data": "TODO: handle images"};
        App.data.add_to_queue(data);        
      } else {
        // if selftext doesn't exists, request the actual page and load it.
        console.log("HTML post for: "+post.url);
        App.post.get_url_content(post.url, function(response_data) {
          var content = response_data.content;
          // remove xml elements from content
          content = content.replace(/<.*?>/g, "");
          // remove \n and \t from content
          content = content.replace(/\n/g, "").replace(/\t/g, "");
          // remove any special characters
          content = content.replace(/&#x[0-9a-zA-Z]*?;/g, "");
          // replace &lt;, &gt;, &amp;
          content = content.replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&amp;/g, "&");
          console.log(content);
          var data = {"post_data": 1,
                      "index": index,
                      "data": content.slice(0, 1024)};
          App.data.add_to_queue(data);
        });
      }
    }
  }
);

// store functions in a global dict namespace
App = {};
// data objects for sending to Pebble
App.data = {};
App.data.queue = []; //queue of data dict to send
App.data.is_sending = false;
App.data._on_send_ack = function(e) {
  // ack - send next message in queue
  if (App.data.queue.length) {
    App.data.send(App.data.queue.shift());
  } else {
    App.data.is_sending = false;
  }    
};
App.data.send = function(data) { // should be a dict
  App.data.is_sending = true;
  Pebble.sendAppMessage(data, 
                        App.data._on_send_ack,
                        App.data._on_send_ack);
};
App.data.add_to_queue = function(data) {
  if (App.data.is_sending) {
    // currently sending - add to queue
    App.data.queue.push(data);
  } else {
    // send immediately
    App.data.send(data);
  }
};
// functions to get data about posts
App.post = {};
App.post.posts = [];
App.post.get = function() {
  var req = new XMLHttpRequest();
  req.open('GET', 'http://reddit.com/r/all.json', true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if(req.status == 200) {
        var response = JSON.parse(req.responseText);
        var num_children = response.data.children.length;
        for (var i=0; i<num_children; i++) {
          var post = response.data.children[i].data;
          App.post.posts.push(post);
          if (i<10) {
            // send the first ten messages to the watch
            var data = {"post": 1, 
                        "index": i,
                        "subreddit": post.subreddit,
                        "author": post.author,
                        "title": post.title};
            App.data.add_to_queue(data);
          }
        }
      } else { console.error("Error requesting posts from Reddit"); }
    }
  };
  req.send(null);
};
App.post.readability_key = "04a61859f5a6f927d930b0b85ccddcf02ae72630";
App.post.get_url_content = function(url, onload) {
  // request content from url
  console.log("Requesting url: "+url);
  var req = new XMLHttpRequest();
  var read_url = "https://www.readability.com/api/content/v1/parser?url="+url+"&token="+App.post.readability_key;
  req.open('GET', read_url, true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if(req.status == 200) {
        console.log("Readability response for url: "+req.responseText);
        onload(JSON.parse(req.responseText));
      } else {
        console.error("Failed to open url: "+post.url);
      }
    }
  };
  req.send(null);
};

