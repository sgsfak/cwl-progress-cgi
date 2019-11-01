# CWL Progress CGI

A [CGI](https://en.wikipedia.org/wiki/Common_Gateway_Interface) for getting the 
status of a workflow execution in [Cromwell](https://github.com/broadinstitute/cromwell)

It is actually meant to be used as a [Slack "slash" command](https://api.slack.com/interactivity/slash-commands)
endpoint...

You can deploy it e.g. using [Caddy](https://caddyserver.com/v1/docs/http.cgi),
or [Apache](https://httpd.apache.org/docs/2.4/howto/cgi.html), 
or [lighttpd](https://redmine.lighttpd.net/projects/lighttpd/wiki/Docs_ModCGI)