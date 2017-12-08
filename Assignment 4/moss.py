import mosspy

userid = 987654321

m = mosspy.Moss(userid, "python")

m.addBaseFile("sfs_api.c")
m.addBaseFile("sfs_api2.c")

# Submission Files
m.addFile("submission/a01-sample.py")
m.addFilesByWildcard("submission/a01-*.py")

url = m.send() # Submission Report URL

print ("Report Url: " + url)

# Save report file
m.saveWebPage(url, "submission/report.html")

# Download whole report locally including code diff links
mosspy.download_report(url, "submission/report/", connections=8)
