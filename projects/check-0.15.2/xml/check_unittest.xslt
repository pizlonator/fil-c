<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:c="http://check.sourceforge.net/ns">

<!--

bart@vankuik.nl, 13-MAR-2006

You can use this xslt on the commandline as follows:
$ xsltproc check_unittest.xslt check_output.xml > check_output.html

-->

<xsl:output method="html"/>

<xsl:template match="c:testsuites">
  <html><title>Test results</title><body>
  <p>
  <table width="100%" border="1" cellpadding="0">
  <tr><td colspan="7" style='background:#e5E5f5'><h1>Unittests</h1></td></tr>
    <xsl:apply-templates/>
  </table></p>
  </body></html>
</xsl:template>

<xsl:template match="c:suite">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="c:title">
  <tr>
  <th colspan="7">Test suite <xsl:apply-templates/></th>
  </tr>
  <tr>
  <td>Path</td>
  <td>Filename</td>
  <td>Test ID</td>
  <td>Iteration</td>
  <td>Duration</td>
  <td>Description</td>
  <td>Message</td>
  </tr>
</xsl:template>

<xsl:template match="c:datetime">
  <tr><th colspan="7">Test ran at: <xsl:apply-templates/></th></tr>
</xsl:template>

<xsl:template match="c:test[@result='success']">
  <tr bgcolor="green"><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="c:test[@result='error']">
  <tr bgcolor="red"><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="c:test[@result='failure']">
  <tr bgcolor="red"><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="c:path">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:fn">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:id">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:iteration">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:description">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:message">
  <td><pre><tt><xsl:apply-templates/></tt></pre></td>
</xsl:template>

<xsl:template match="c:test/c:duration">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="c:duration">
  <tr><th colspan="7">Test duration: <xsl:apply-templates/> seconds</th></tr>
</xsl:template>

</xsl:stylesheet>

