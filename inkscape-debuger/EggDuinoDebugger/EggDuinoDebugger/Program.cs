using System;
using System.IO;
using System.IO.Ports;

class Program
{
    static async Task Main(string[] args)
    {
        string filePath = Path.Combine("instruments", "zajíc.txt"); // Cesta k textovému souboru s příkazy
        try
        {
            Console.WriteLine($"Using file: '{filePath}'");
            using (SerialPort serialPort = new SerialPort("COM3", 9600)) // Nahraďte "COM3" správným portem
            {
                serialPort.Open();

                int lineNumber = 0; // Pro sledování čísla řádku
                foreach (string command in File.ReadLines(filePath))
                {
                    lineNumber++;

                    var parts = command.Split(',');
                    Console.Write($"{lineNumber,6}: '{command}'...");
                    serialPort.WriteLine(command + "\r"); // Poslat příkaz

                    if (parts[0] == "SM")
                    {
                        await Task.Delay(int.Parse(parts[1]));
                    }

                    string response = serialPort.ReadLine(); // Čekat na odpověď
                    Console.WriteLine($"reponse: '{response}'");
                    if (response == "Unknown CMD")
                    {
                        break; // Přerušit cyklus, pokud odpověď není OK
                    }
                }
            }

            Console.WriteLine("Finished!");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Došlo k chybě: {ex.Message}");
        }
    }
}