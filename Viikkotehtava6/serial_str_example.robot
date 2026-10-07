*** Settings ***
Library   String
Library   SerialLibrary

*** Variables ***
${com}   	COM14
${baud} 	115200
${board}	nRF
${correct_seq}      000120X
${fail_seq}			001067X
${fail_seq2}		00067X
${TIME_VALUE_ERROR}    -3X
${TIME_LEN_ERROR}      -1X

*** Test Cases ***
Connect Serial
	Log To Console  Connecting to ${board}
	Add Port  ${com}  baudrate=${baud}  encoding=ascii
	Port Should Be Open  ${com}
	Reset Input Buffer
	Reset Output Buffer
	
Serial Value Error
	Write Data   ${fail_seq}   encoding=ascii 
	Log To Console   Send sequence ${fail_seq}

	# vastaanotetaan merkkijono kunnes lopetusmerkki X (58) 
	${read} =   Read Until   terminator=58   encoding=ascii 

	# konsolille näkyviin vastaanotettu merkkijono
	Log To Console   Received ${read}
	
	# vertaillaan merkkijonoa
	Should Be Equal As Strings   ${read}    ${TIME_VALUE_ERROR}
	Log To Console   Tested ${read} is same as ${TIME_VALUE_ERROR}

	
Serial Len Error
	Write Data   ${fail_seq2}   encoding=ascii 
	Log To Console   Send sequence ${fail_seq2}

	# vastaanotetaan merkkijono kunnes lopetusmerkki X (58) 
	${read} =   Read Until   terminator=58   encoding=ascii 

	# konsolille näkyviin vastaanotettu merkkijono
	Log To Console   Received ${read}
	
	# vertaillaan merkkijonoa
	Should Be Equal As Strings   ${read}    -1X
	Log To Console   Tested ${read} is same as -1X

	# astetta hankalampi tehdä testaus numeroina
	# koska lopetusmerkki X pitää ensin poistaa merkkijonosta
	# tai vaihtaa lopetusmerkki esim \0
	# Should Be Equal As Integers   ${read}    -1
	
Serial Correct Sequence
	Write Data   ${correct_seq}   encoding=ascii 
	Log To Console   Send correct sequence ${correct_seq}

	# vastaanotetaan merkkijono kunnes lopetusmerkki X (58) 
	${read} =   Read Until   terminator=58   encoding=ascii 

	# konsolille näkyviin vastaanotettu merkkijono
	Log To Console   Received ${read}
	
	# vertaillaan merkkijonoa
	Should Be Equal As Strings   ${read}    80X
	Log To Console   Tested ${read} is same as 80X
	
Disconnect Serial
	Log To Console  Disconnecting ${board}
	[TearDown]  Delete Port  ${com}


	
	
	

