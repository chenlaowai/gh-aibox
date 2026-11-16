using System.ComponentModel.DataAnnotations;

namespace HNKC.AIBox.Models.Attributes;

/// <summary>
/// 特性：自动去除字符串首尾空格
/// </summary>
[AttributeUsage(AttributeTargets.Property, AllowMultiple = false)]
public class TrimAttribute : ValidationAttribute
{
    protected override ValidationResult IsValid(object value, ValidationContext validationContext)
    {
        if (value is string stringValue && !string.IsNullOrEmpty(stringValue))
        {
            if (validationContext.MemberName != null)
            {
                var property = validationContext.ObjectType.GetProperty(validationContext.MemberName);
                if (property != null && property.CanWrite)
                {
                    property.SetValue(validationContext.ObjectInstance, stringValue.Trim());
                }
            }
        }

        return ValidationResult.Success;
    }
}