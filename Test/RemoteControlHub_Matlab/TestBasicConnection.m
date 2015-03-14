t = tcpip('localhost', 7770);
set( t, 'InputBufferSize', 1e6 );
fopen(t);
t.Status
fprintf(t, 'RagdollController RCH: CONNECT Owen');

xmlDocument = sprintf( '<foo1>\n<bar1>abc</bar1>\n<bar2>def</bar2>\n</foo1>\n' );

xmlHeader = sprintf( 'XML_DOCUMENT_BEGIN %.10d\n', 0 );
xmlHeader = sprintf( 'XML_DOCUMENT_BEGIN %.10d\n', length(xmlHeader) + length(xmlDocument) );
xmlBlock = [xmlHeader xmlDocument];

fwrite(t, xmlBlock);

while true
    if( t.BytesAvailable > 0 )
      str = fread( t, t.BytesAvailable );
      fprintf(char(str))
    end
end 
