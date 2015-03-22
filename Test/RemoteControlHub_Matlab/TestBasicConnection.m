t = tcpip('localhost', 7770);
set( t, 'InputBufferSize', 1e6 );
fopen(t);
t.Status
fprintf(t, 'RagdollController RCH: CONNECT Owen');

xmlHeader = sprintf( 'XML_DOCUMENT_BEGIN\n' );
xmlFooter = sprintf( 'XML_DOCUMENT_END\n' );
xmlDocument = sprintf( '<foo1>\n<bar1>abc</bar1>\n<bar2>def</bar2>\n</foo1>\n' );

xmlBlock = [xmlHeader xmlDocument xmlFooter];

fwrite(t, xmlBlock);

while true
    if( t.BytesAvailable > 0 )
      str = fread( t, t.BytesAvailable );
      fprintf(char(str))
    end
end 
